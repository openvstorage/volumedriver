// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

#include "EventCollector.h"
#include "MDSTestSetup.h"
#include "VolManagerTestSetup.h"

#include "../Api.h"
#include "../ArakoonMetaDataBackend.h"
#include "../CachedMetaDataStore.h"
#include "../CombinedTLogReader.h"
#include "../DataStoreNG.h"
#include "../FailOverCacheClientInterface.h"
#include "../SCOCache.h"
#include "../TokyoCabinetMetaDataBackend.h"
#include "../VolManager.h"
#include "../failovercache/FailOverCacheAcceptor.h"
#include "../metadata-server/Manager.h"
#include "../metadata-server/ServerConfig.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <cerrno>
#include <semaphore.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>

#include <fawltyfs/corba-daemon/Literals.h>

#include <youtils/DimensionedValue.h>
#include <youtils/ScopeExit.h>

#include <backend/BackendInterface.h>
#include <backend/GarbageCollector.h>
#include <backend/Local_Connection.h>
#include <backend/SimpleFetcher.h>

namespace volumedriver
{

using youtils::BarrierTask;

namespace ara = arakoon;
namespace be = backend;
namespace bpt = boost::property_tree;
namespace mds = metadata_server;
namespace vdt = volumedrivertest;
namespace yt = youtils;

using namespace initialized_params;

VolumeRandomIOThread::VolumeRandomIOThread(Volume& vol,
                                           const std::string &writePattern,
                                           const CheckReadResult check)
    : vol_(vol)
    , writePattern_(writePattern)
    , stop_(false)
    , bufsize_(4096)
    , check_(check)
    , bufv_(bufsize_)
{
    if (writePattern_.empty())
    {
        throw fungi::IOException("Please provide a non empty pattern for writing");
    }

    thread_.reset(new boost::thread(boost::bind(&VolumeRandomIOThread::run, this)));
}

VolumeRandomIOThread::~VolumeRandomIOThread()
{
    EXPECT_NO_THROW(stop());
}

void VolumeRandomIOThread::stop()
{
    stop_ = true;
    if (thread_.get())
    {
        thread_->join();
    }
}

void VolumeRandomIOThread::run()
{
    unsigned int seed = time(0);
    uint64_t lbaMax = vol_.getLBACount() - bufsize_;

    while (!stop_)
    {
        uint64_t lba = rand_r(&seed) % (vol_.getSize() / vol_.getLBASize());
        bool read = (lba % 2 == 0);

        if (lba > lbaMax)
        {
            lba = lbaMax;
        }
        if (not read)
        {
            for (size_t i = 0; i < bufsize_; ++i)
            {
                bufv_[i] = writePattern_[i % writePattern_.size()];
            }
        }

        try
        {
            if (read)
            {
                vol_.read(lba, &bufv_[0], bufsize_);
                if(T(check_))
                {
                    if(bufv_[0])
                    {
                        for(size_t i = 0; i < bufsize_; ++i)
                        {

                            ASSERT_EQ(bufv_[i], writePattern_[i % writePattern_.size()]);
                        }
                    }
                    else
                    {
                        for(uint8_t i : bufv_)
                        {
                            ASSERT_EQ(i,0U);
                        }
                    }

                }

            }
            else
            {
                vol_.write(lba, &bufv_[0], bufsize_);
            }
        }
        catch (TransientException &)
        {
            sleep(1);
        }
    }
}

// Should be in the class but my compiler sometimes barfs.
const uint32_t
VolumeWriteThread::bufsize = 4096;

VolumeWriteThread::VolumeWriteThread(Volume& vol,
                                     const std::string& pattern,
                                     double throughput /* Megs per sec */,
                                     uint64_t max_writes)
    : thread_(nullptr)
    , stop_(false)
    , pattern_(pattern)
    , vol_(vol)
    , max_writes_(max_writes)
    , written_(0)
{
    // safety check
    VERIFY(bufsize == vol_.getClusterSize());
    VERIFY(throughput >= 1);

    if(pattern.empty())
    {
        throw fungi::IOException("Please provide a non empty pattern for writing");
    }

    thread_.reset(new boost::thread(boost::bind(&VolumeWriteThread::run, this)));
}

VolumeWriteThread::~VolumeWriteThread()
{
    try
    {
        stop();
    }
    catch (std::exception& e)
    {
        EXPECT_TRUE(false) << "Failed to stop thread: " << e.what();
    }
    catch (...)
    {
        EXPECT_TRUE(false) << "Failed to stop thread: unknown exception";
    }
}

void
VolumeWriteThread::stop()
{
    stop_ = true;
    if (thread_.get() != 0)
    {
        thread_->join();
    }
}

void
VolumeWriteThread::waitForFinish()
{
    thread_->join();
}

void
VolumeWriteThread::run()
{
    uint64_t lba = 0;
    const uint64_t volSize = vol_.getSize();
    const uint64_t lbaSize = vol_.getLBASize();
    const uint64_t increment = bufsize / vol_.getLBASize();
    assert(increment == 8);

    uint8_t buf[bufsize];
    for(size_t i = 0; i < bufsize; ++i)
    {
        buf[i] = pattern_[i%pattern_.size()];
    }

    while(not stop_ and
          maxNotExceeded())
    {
        if(lba * lbaSize > volSize)
        {
            lba = 0;
        }
        try {
            vol_.write(lba,buf, bufsize);
        } catch (TransientException &e) {
            LOG_DEBUG("" << e.what() << ", retrying");
            sleep(1);
            continue;
        }

        lba += increment;
        written_ += bufsize;
    }
}

VolumeCheckerThread::VolumeCheckerThread(Volume* v,
                                         const std::string& pattern,
                                         size_t block_size,
                                         size_t read_size,
                                         unsigned retries)
    : v_(v)
    , pattern_(pattern)
    , buf_(block_size)
    , read_size_(read_size)
    , retries_(retries)
    , thread_(nullptr)
{
    EXPECT_LE(block_size, read_size) << "Fix your test";
    EXPECT_EQ(0U, read_size_ % read_size) << "Fix your test";
    EXPECT_EQ(0U, buf_.size() % v_->getLBASize()) << "Fix your test";
    EXPECT_FALSE(pattern_.empty()) << "Fix your test";

    for (size_t i = 0; i < buf_.size(); ++i)
    {
        buf_[i] = pattern_[i % pattern.size()];
    }

    thread_.reset(new boost::thread(boost::bind(&VolumeCheckerThread::run, this)));
}

VolumeCheckerThread::~VolumeCheckerThread()
{
    EXPECT_NO_THROW(waitForFinish());
}

void
VolumeCheckerThread::run()
{
    for (size_t i = 0; i < read_size_; i += buf_.size())
    {
        unsigned retries = retries_;
        std::vector<byte> read_buf(buf_.size(), 'x');

        do
        {
            try
            {
                v_->read(i / v_->getLBASize(),
                         &read_buf[0],
                         read_buf.size());
                EXPECT_TRUE(buf_ == read_buf) <<
                    "lba " << (i / v_->getLBASize()) <<
                    ", buf size " << read_buf.size() <<
                    ", ref pattern " << pattern_;
                break;
            }
            catch (TransientException &e)
            {
                LOG_DEBUG("caught transient exception " << e.what());
                if (retries-- > 0)
                {
                    LOG_DEBUG("retrying");
                    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
                }
                else
                {
                    LOG_DEBUG("giving up");
                    throw;
                }
            }
        }
        while (true);
    }
}

void
VolumeCheckerThread::waitForFinish()
{
    if (thread_.get() != 0)
    {
        thread_->join();
    }
}

class ThrowingTask :
    public backend_task::TaskBase
{
public:
    explicit ThrowingTask(Volume* vol)
        : backend_task::TaskBase(vol,BarrierTask::T)
    {}

    virtual const std::string&
    getName() const
    {
        static const std:: string t("ThrowingTask");
        return t;
    }

    virtual void
    run(int /*threadID*/)
    {
        throw fungi::IOException("ThrowingTask");
    }
};

class WaitForThisTask
    : public backend_task::TaskBase
{
public:
    WaitForThisTask(sem_t* sem,
                    VolumeInterface* vol)
        : backend_task::TaskBase(vol, BarrierTask::T),
          sem_(sem)
    {}

    virtual const std::string&
    getName() const
    {
        static const std:: string t("WaitTask");
        return t;
    }

    virtual void
    run(int /*threadID*/)
    {
        sem_post(sem_);
    }

private:
    sem_t* sem_;
};

class BSemTask
    : public backend_task::TaskBase
{
public:
    explicit BSemTask(VolumeInterface* vol)
        : backend_task::TaskBase(vol, BarrierTask::F)
    {
        if(sem_init(&sem_,0,0))
        {
            throw fungi::IOException("Could not initialize semaphore");
        }
        if(sem_init(&sem2_,0,0))
        {
            throw fungi::IOException("Could not initialize semaphore");
        }

    }

    ~BSemTask()
    {
        sem_destroy(&sem_);
        sem_destroy(&sem2_);
    }

    virtual const
    std::string & getName() const
    {
        static const std::string t("BSemTask");
        return t;
    }

    virtual void
    run(int /*threadID*/)
    {
        LOG_INFO("Posting on sem2)");
        sem_post(&sem2_);
        LOG_INFO("Wating for post on sem_");
        sem_wait(&sem_);
    }
    void
    cont()
    {
        LOG_INFO("Continue");
        sem_post(&sem_);
    }

    virtual void
    waitForRun()
    {
        LOG_INFO("Waiting on sem2_");

        sem_wait(&sem2_);
    }

private:
    DECLARE_LOGGER("BSemTask");

    sem_t sem_;
    sem_t sem2_;
};

VolManagerTestSetup::VolManagerTestSetup(const VolManagerTestSetupParameters& params)
    : be::BackendTestSetup()
    , FailOverCacheTestSetup(GetParam().foc_in_memory() ?
                             boost::none :
                             boost::optional<fs::path>(yt::FileUtils::temp_path(params.name()) / "foc"))
    , testName_(params.name())
    , directory_(yt::FileUtils::temp_path(testName_))
    , configuration_(directory_ / "configuration")
    , sc_mp1_size_(yt::DimensionedValue(params.sco_cache_mp1_size()).getBytes())
    , sc_mp2_size_(yt::DimensionedValue(params.sco_cache_mp2_size()).getBytes())
    , sc_trigger_gap_(yt::DimensionedValue(params.sco_cache_trigger_gap()).getBytes())
    , sc_backoff_gap_(yt::DimensionedValue(params.sco_cache_backoff_gap()).getBytes())
    , sc_clean_interval_(params.sco_cache_cleanup_interval())
    , open_scos_per_volume_(params.open_scos_per_volume())
    , dstore_throttle_usecs_(params.data_store_throttle_usecs())
    , foc_throttle_usecs_(params.dtl_throttle_usecs())
    , useFawltyMDStores_(params.use_fawlty_md_stores())
    , useFawltyTLogStores_(params.use_fawlty_tlog_stores())
    , useFawltyDataStores_(params.use_fawlty_data_stores())
    , dtl_check_interval_(params.dtl_check_interval())
    , event_collector_(std::make_shared<vdt::EventCollector>())
    , volManagerRunning_(false)
    , num_threads_(params.backend_threads())
    , num_scos_in_tlog_(params.scos_per_tlog())
{
    EXPECT_LE(sc_trigger_gap_, sc_backoff_gap_) <<
        "invalid trigger gap + backoff gap for the SCO cache specified, fix your test!";
    EXPECT_LT(sc_backoff_gap_, std::min<uint64_t>(sc_mp1_size_,
                                                  sc_mp2_size_)) <<
        "invalid backoff gap specified for the SCO cache, fix your test!";

    sc_mp1_path_ = directory_ / "scocache_mp1";
    sc_mp2_path_ = directory_ / "scocache_mp2";
}

VolManagerTestSetup::~VolManagerTestSetup()
{
    // If you, dear esteemed reader, are wondering why this wasn't `= default'ed in the
    // header, allow yours truly to most humbly remind you of the
    // std::unique_ptr<SomeForwardDeclaredType> member variables.
}

fs::path
VolManagerTestSetup::setupClusterCacheDevice(const std::string& name,
                                             int size)
{
    fs::path device = directory_ / name;
    int fd = -1;
    fd = open(device.string().c_str(),
              O_RDWR| O_CREAT | O_EXCL,
              S_IWUSR|S_IRUSR);
    if(fd < 0)
    {
        LOG_ERROR("Couldn't open file " << device << ", " << strerror(errno));
        throw fungi::IOException("Couldn't open file");
    }

    int ret = posix_fallocate(fd, 0, size);
    if (ret != 0)
    {
        LOG_ERROR("Could not allocate file " << device << " with size " << size << ", " << strerror(ret));
        throw fungi::IOException("Could not allocate file",
                                 device.string().c_str(),
                                 ret);
    }
    fchmod(fd,00600);

    close(fd);
    return device;
}

static const fs::path&
ensure_directory(const fs::path& dir)
{
    fs::create_directories(dir);
    return dir;
}

void
VolManagerTestSetup::setup_fawlty_directory(fs::path dir,
                                            Fawlty::FileSystem_var& var,
                                            const std::string& name)
{
    fs::path fawlty_dir(dir.string() +  "_fawlty");
    ensure_directory(dir);
    ensure_directory(fawlty_dir);

    VERIFY(not CORBA::is_nil(fawlty_ref_));
    CORBA::Object_var obj = fawlty_ref_->create(fawlty_dir.string().c_str(),
                                                dir.string().c_str(),
                                                name.c_str());
    VERIFY(not CORBA::is_nil(obj));
    var = Fawlty::FileSystem::_narrow(obj);
    VERIFY(not CORBA::is_nil(var));
    var->mount();
}


CORBA::Object_ptr
VolManagerTestSetup::getObjectReference(CORBA::ORB_ptr orb)
{
    CosNaming::NamingContext_var rootContext;

    try
    {
        // Obtain a reference to the root context of the Name service:
        CORBA::Object_var obj;
        obj = orb->resolve_initial_references("NameService");

        // Narrow the reference returned.
        rootContext = CosNaming::NamingContext::_narrow(obj);
        if( CORBA::is_nil(rootContext) )
        {
            LOG_FATAL("Failed to narrow the root naming context.");

            return CORBA::Object::_nil();
        }
    }
    catch (CORBA::NO_RESOURCES&)
    {
                LOG_FATAL("Caught NO_RESOURCES exception. You must configure omniORB with the location of the naming service.");
        return CORBA::Object::_nil();

    }
    catch(CORBA::ORB::InvalidName& ex)
    {
        // This should not happen!
        LOG_FATAL("Service required is invalid [does not exist].");
        return CORBA::Object::_nil();
    }

    // Create a name object, containing the name test/context:
    CosNaming::Name name;
    name.length(2);

    name[0].id   = (const char*) TEST_NAME;
    name[0].kind = (const char*) TEST_KIND;
    name[1].id   = (const char*) OBJECT_NAME;
    name[1].kind = (const char*) OBJECT_KIND;
    // Note on kind: The kind field is used to indicate the type
    // of the object. This is to avoid conventions such as that used
    // by files (name.type -- e.g. test.ps = postscript etc.)

    try
    {
        // Resolve the name to an object reference.
        return rootContext->resolve(name);
    }
    catch(CosNaming::NamingContext::NotFound& ex)
    {
        // This exception is thrown if any of the components of the
        // path [contexts or the object] aren't found:
        LOG_INFO("Context not found.");
    }
    catch(CORBA::TRANSIENT& ex)
    {
        LOG_FATAL( "Caught system exception TRANSIENT -- unable to contact the naming service.");
        LOG_FATAL("Make sure the naming server is running and that omniORB is configured correctly.");
    }
    catch(CORBA::SystemException& ex)
    {
        LOG_FATAL("Caught a CORBA::" << ex._name()  << " while using the naming service.");
    }
    return CORBA::Object::_nil();
}

void
VolManagerTestSetup::setup_corba_()
{
    if(T(useFawltyDataStores_) or
       T(useFawltyTLogStores_) or
       T(useFawltyMDStores_))
    {
        fs::path ior_file = FileUtils::create_temp_file_in_temp_dir("ior_output");
        ALWAYS_CLEANUP_FILE(ior_file);

        std::string command("corba_server --logsink=/tmp/corba_log --use-naming-service=false --ior-file=" + ior_file.string());

        //        ::system("corba_server > corba_server_log.txt 2>&1 &");
        pstream_.open(command, redi::pstreambuf::pstdout | redi::pstreambuf::pstderr);

        for(int sleep_time = 1; sleep_time < 10; ++sleep_time)
        {
            if(pstream_.is_open() and
               fs::exists(ior_file) and
               fs::file_size(ior_file) > 0)
            {
                break;
            }
            sleep(sleep_time);
        }

        if(pstream_.rdbuf()->exited() or
           not fs::exists(ior_file))
        {
            LOG_FATAL("Could not start the fawlty corba server, exiting now");
            abort();
        }


        int argc = 1;

        const char* argv[] = {
            "volumedriver_test",
            0
        };

        orb_ = CORBA::ORB_init(argc, (char**)argv);
        VERIFY(not CORBA::is_nil(orb_));
        fs::ifstream ior_stream(ior_file);

        std::string ior_str;
        while(ior_str.empty())
        {
            ior_stream >> ior_str;
        }

        CORBA::Object_var obj = orb_->string_to_object(ior_str.c_str());

        VERIFY(not CORBA::is_nil(obj));
        fawlty_ref_ = Fawlty::FileSystemFactory::_narrow(obj);
        VERIFY(not CORBA::is_nil(fawlty_ref_));

    }
}

void
VolManagerTestSetup::setup_arakoon_()
{
    if (ara::ArakoonTestSetup::useArakoon())
    {
        arakoon_test_setup_ =
            std::make_shared<ara::ArakoonTestSetup>(directory_ / "arakoon");
        arakoon_test_setup_->setUpArakoon();
    }
}

void
VolManagerTestSetup::teardown_arakoon_()
{
    if (arakoon_test_setup_ != nullptr)
    {
        arakoon_test_setup_->tearDownArakoon();
        arakoon_test_setup_ = nullptr;
    }
}

void
VolManagerTestSetup::setup_md_backend_()
{
    ASSERT_TRUE(mdstore_test_setup_ == nullptr);
    ASSERT_TRUE(cm_ != nullptr);

    setup_arakoon_();
    mds_test_setup_ = std::make_shared<vdt::MDSTestSetup>(directory_ / "mds");
    mds_server_config_ =
        std::make_unique<mds::ServerConfig>(mds_test_setup_->next_server_config());
    mdstore_test_setup_ =
        std::make_unique<vdt::MetaDataStoreTestSetup>(arakoon_test_setup_,
                                                      mds_server_config_->node_config);
}

void
VolManagerTestSetup::teardown_md_backend_()
{
    mdstore_test_setup_.reset();
    mds_server_config_.reset();
    mds_test_setup_ = nullptr;
    teardown_arakoon_();
}

void
VolManagerTestSetup::setup_stores_()
{
    if(T(useFawltyDataStores_))
    {
        setup_fawlty_directory(sc_mp1_path_,
                               sdstore1_,
                               "datastore_1");

        setup_fawlty_directory(sc_mp2_path_,
                               sdstore2_,
                               "datastore_2");
    }
    else
    {
        fs::create_directories(sc_mp1_path_);
        fs::create_directories(sc_mp2_path_);
    }

    if(T(useFawltyMDStores_))
    {
        setup_fawlty_directory(directory_ / "metadatastores",
                               mdstores_,
                               "metadatastores");

    }

    if(T(useFawltyTLogStores_))
    {
        setup_fawlty_directory(directory_ / "tlogs",
                               tlogs_,
                               "tlogs");
    }
}

void
VolManagerTestSetup::SetUp()
{
    //
    fs::remove_all(directory_);

    fs::create_directories(directory_);
    fs::create_directories(directory_ / "failovercache");
    fs::create_directories(configuration_);
    fs::create_directories(directory_ / "readcache_serialization");

    setenv("VD_CONFIG_DIR", configuration_.string().c_str(), 1);

    setup_corba_();

    setup_stores_();

    initialize_connection_manager();

    setup_md_backend_();

    ASSERT_FALSE(volManagerRunning_);

    startVolManager();
}

void
VolManagerTestSetup::add_failure_rule(Fawlty::FileSystem_var& fawlty_fs,
                                      unsigned rule_id,
                                      const std::string& path_regex,
                                      const std::set<fawltyfs::FileSystemCall>& calls,
                                      int ramp_up,
                                      int duration,
                                      int error_code)
{
    VERIFY(not CORBA::is_nil(fawlty_fs));
    fawlty_fs->addFailureRule(rule_id,
                              path_regex.c_str(),
                              toCorba(calls),
                              ramp_up,
                              duration,
                              error_code);

}

void
VolManagerTestSetup::remove_failure_rule(Fawlty::FileSystem_var& fawlty_fs,
                                         unsigned rule_id)
{
    fawlty_fs->removeFailureRule(rule_id);
}

void
VolManagerTestSetup::teardown_stores_()
{
    if(not CORBA::is_nil(mdstores_))
    {
        mdstores_->umount();
    }

    if(not CORBA::is_nil(tlogs_))
    {
        tlogs_->umount();
    }

    if(not CORBA::is_nil(sdstore1_))
    {
        sdstore1_->umount();
    }

    if(not CORBA::is_nil(sdstore2_))
    {
        sdstore2_->umount();
    }

    if(not CORBA::is_nil(fawlty_ref_))
    {
        fawlty_ref_->shutdown(true);
    }
    else if (pstream_.is_open())
    {
        pstream_.rdbuf()->kill(SIGKILL);
    }

    if(not CORBA::is_nil(orb_))
    {
        orb_->destroy();
    }
}

void
VolManagerTestSetup::TearDown()
{
    if (volManagerRunning_)
    {
        VolManager::stop();
    }

    teardown_md_backend_();

    uninitialize_connection_manager();

    teardown_stores_();

    if(pstream_.is_open())
    {
        for(unsigned sleep_time = 1; sleep_time < 10; ++sleep_time)
        {
            if(pstream_.rdbuf()->exited())
            {
                break;
            }
            sleep(sleep_time);
        }
        if(not pstream_.rdbuf()->exited())
        {
            LOG_FATAL("Could not shutdown the fawlty process cleanly");
            abort();
        }
    }

    fs::remove_all(directory_);
}

void
VolManagerTestSetup::temporarilyStopVolManager()
{
    if (volManagerRunning_)
    {
        VolManager::stop();
        volManagerRunning_ = false;
    }
}

void
VolManagerTestSetup::getCacheConfig(bpt::ptree& pt) const
{
    MountPointConfigs mp_configs;
    mp_configs.push_back(MountPointConfig(sc_mp1_path_.string(),sc_mp1_size_));
    mp_configs.push_back(MountPointConfig(sc_mp2_path_.string(),sc_mp2_size_));
    bpt::ptree empty;

    PARAMETER_TYPE(scocache_mount_points)(mp_configs).persist(pt);
    PARAMETER_TYPE(trigger_gap)(yt::DimensionedValue(sc_trigger_gap_)).persist(pt);
    PARAMETER_TYPE(backoff_gap)(yt::DimensionedValue(sc_backoff_gap_)).persist(pt);
}

void
VolManagerTestSetup::startVolManager()
{
    if (!volManagerRunning_)
    {
        bpt::ptree pt;
        BackendTestSetup::backend_config().persist_internal(pt, ReportDefault::T);

        getCacheConfig(pt);

        std::vector<MountPointConfig> vec;
        if(GetParam().use_cluster_cache())
        {
            yt::DimensionedValue theFirst("10MiB");
            yt::DimensionedValue theSecond("20MiB");

            cc_mp1_path_ = setupClusterCacheDevice("readcachethefirst",
                                                theFirst.getBytes());

            cc_mp2_path_ = setupClusterCacheDevice("readcachethesecond",
                                                theSecond.getBytes());



            vec.push_back(MountPointConfig(cc_mp1_path_.string(),
                                           theFirst.getBytes()));
            vec.push_back(MountPointConfig(cc_mp2_path_.string(),
                                           theFirst.getBytes()));
        }

        PARAMETER_TYPE(clustercache_mount_points)(vec).persist(pt);

        PARAMETER_TYPE(read_cache_serialization_path)((directory_ / "metadatastores").string()).persist(pt);
        PARAMETER_TYPE(tlog_path)((directory_ / "tlogs").string()).persist(pt);
        PARAMETER_TYPE(metadata_path)((directory_ / "metadatastores").string()).persist(pt);
        PARAMETER_TYPE(open_scos_per_volume)(open_scos_per_volume_).persist(pt);
        PARAMETER_TYPE(datastore_throttle_usecs)(dstore_throttle_usecs_).persist(pt);
        PARAMETER_TYPE(clean_interval)(sc_clean_interval_).persist(pt);
        PARAMETER_TYPE(num_threads)(num_threads_).persist(pt);
        PARAMETER_TYPE(number_of_scos_in_tlog)(num_scos_in_tlog_).persist(pt);

        {
            const ClusterMultiplier
                cmult(GetParam().cluster_multiplier());
            const ClusterSize
                csize(cmult * VolumeConfig::default_lba_size());

            PARAMETER_TYPE(default_cluster_size)(static_cast<uint32_t>(csize)).persist(pt);
        }

        PARAMETER_TYPE(debug_metadata_path)((directory_ / fs::path("dump_on_halt_dir")).string()).persist(pt);
        PARAMETER_TYPE(dtl_check_interval_in_seconds)(dtl_check_interval_.count()).persist(pt);

        const mds::ServerConfigs scfgs{ *mds_server_config_ };
        mds_test_setup_->make_manager_config(pt,
                                             scfgs);

        //   pt.put("version", 1);

        restartVolManager(pt);
    }
}

void
VolManagerTestSetup::restartVolManager(const bpt::ptree& pt)
{
    if (not volManagerRunning_)
    {
        VolManager::start(pt,
                          event_collector_);
        VolManager::get()->persistConfiguration((directory_ / "volmanager_config.json").string());

        volManagerRunning_ = true;
    }
}

void
VolManagerTestSetup::checkCurrentBackendSize(Volume& vol)
{
    vol.sync();
    uint64_t size_in_snapshot = vol.getSnapshotManagement().getCurrentBackendSize();
    const OrderedTLogIds
        current_tlogs(vol.getSnapshotManagement().getCurrentTLogs());

    std::shared_ptr<TLogReaderInterface>
        t(CombinedTLogReader::create_backward_reader(VolManager::get()->getTLogPath(vol),
                                                     current_tlogs,
                                                     vol.getBackendInterface()->clone()));
    uint64_t current_size_calculated = 0;
    while(t->nextLocation())
    {
        current_size_calculated += vol.getClusterSize();
    }
    EXPECT_EQ(size_in_snapshot, current_size_calculated) <<
        "Current size in snapshot " <<
        size_in_snapshot << ", Checked from TLogs " <<
        current_size_calculated;
}

void
VolManagerTestSetup::set_cluster_cache_default_behaviour(const ClusterCacheBehaviour b)
{
    VolManager::get()->read_cache_default_behaviour.update(b);
}

void
VolManagerTestSetup::set_cluster_cache_default_mode(const ClusterCacheMode m)
{
    VolManager::get()->read_cache_default_mode.update(m);
}

void
VolManagerTestSetup::waitForThisBackendWrite(VolumeInterface& vol)
{
    // Has changed since there is only 1 thread servicing a queue now.
    // which probably means this whole thing can be simplified now.
    //    const int numthreads = 1;

    sem_t sem;

    sem_init(&sem, 0, 0);

    VolManager::get()->scheduleTask(new WaitForThisTask(&sem, &vol));

    sem_wait(&sem);
    sem_destroy(&sem);
}

void
VolManagerTestSetup::syncToBackend(Volume& vol)
{
    scheduleBackendSync(vol);
    waitForThisBackendWrite(vol);
}

fs::path
VolManagerTestSetup::getVolDir(const std::string &volName) const
{
    return  directory_ / volName;
}

fs::path
VolManagerTestSetup::getDCDir(const std::string& volName) const
{
    return directory_ / "failovercache" / volName;
}

fs::path
VolManagerTestSetup::getCurrentTLog(const VolumeId& volid) const
{
    VolManager *vm = VolManager::get();
    SharedVolumePtr v;
    fungi::ScopedLock m(vm->getLock_());
    v = vm->findVolume_(volid);
    return v->getSnapshotManagement().getCurrentTLogPath();
}

void
VolManagerTestSetup::getTLogsNotInBackend(const VolumeId& volid,
                                      OrderedTLogIds& tlogs) const
{
    VolManager *vm = VolManager::get();
    SharedVolumePtr v;
    fungi::ScopedLock m(vm->getLock_());
    v = vm->findVolume_(volid);
    return v->getSnapshotManagement().getTLogsNotWrittenToBackend(tlogs);
}

void
VolManagerTestSetup::setVolumeRole(const Namespace ns,
                                   VolumeConfig::WanBackupVolumeRole role)
{
    BackendInterfacePtr bi(VolManager::get()->createBackendInterface(ns));
    const fs::path
        p(FileUtils::create_temp_file_in_temp_dir(VolumeConfig::config_backend_name));
    bi->read(p, VolumeConfig::config_backend_name, InsistOnLatestVersion::T);
    VolumeConfig vol_config;
    fs::ifstream ifs(p);
    VolumeConfig::iarchive_type ia(ifs);
    ia & vol_config;
    ifs.close();
    const_cast<VolumeConfig::WanBackupVolumeRole&>(vol_config.wan_backup_volume_role_) = role;
    fs::remove(p);

    Serialization::serializeAndFlush<VolumeConfig::oarchive_type>(p, vol_config);

    bi->write(p, VolumeConfig::config_backend_name, OverwriteObject::T);
}

void
VolManagerTestSetup::restartVolume(const VolumeConfig& cfg,
                                   const PrefetchVolumeData prefetch,
                                   const CheckVolumeNameForRestart check_name,
                                   const IgnoreFOCIfUnreachable ignore_foc_if_unreachable)
{
    fungi::ScopedLock l(VolManager::get()->mgmtMutex_);
    VolManager::get()->backend_restart(cfg.getNS(),
                                       cfg.owner_tag_,
                                       prefetch,
                                       ignore_foc_if_unreachable);
    if(T(check_name))
    {
        __attribute__ ((__unused__)) SharedVolumePtr v =
            VolManager::get()->findVolume_noThrow_(cfg.id_);
    }
}

WriteOnlyVolume*
VolManagerTestSetup::restartWriteOnlyVolume(const VolumeConfig& cfg)
{
    fungi::ScopedLock l(VolManager::get()->mgmtMutex_);
    return VolManager::get()->restartWriteOnlyVolume(cfg.getNS(),
                                                     cfg.owner_tag_);
}

SharedVolumePtr
VolManagerTestSetup::findVolume(const VolumeId& ns)
{
    fungi::ScopedLock l(VolManager::get()->mgmtMutex_);
    return VolManager::get()->findVolume_noThrow_(ns);
}

SharedVolumePtr
VolManagerTestSetup::newVolume(const be::BackendTestSetup::WithRandomNamespace& wrns,
                               const boost::optional<VolumeSize>& volume_size,
                               const boost::optional<SCOMultiplier>& sco_mult,
                               const boost::optional<LBASize>& lba_size,
                               const boost::optional<ClusterMultiplier>& cluster_mult,
                               const boost::optional<size_t>& num_cached_pages)
{
    return newVolume(wrns.ns().str(),
                     wrns.ns(),
                     volume_size,
                     sco_mult,
                     lba_size,
                     cluster_mult,
                     num_cached_pages);
}

SharedVolumePtr
VolManagerTestSetup::newVolume(const std::string& id,
                               const be::Namespace& ns,
                               const boost::optional<VolumeSize>& volume_size,
                               const boost::optional<SCOMultiplier>& sco_mult,
                               const boost::optional<LBASize>& lba_size,
                               const boost::optional<ClusterMultiplier>& cluster_mult,
                               const boost::optional<size_t>& num_cached_pages)
{
    // Push VolumeID out!
    auto params(VanillaVolumeConfigParameters(VolumeId(id),
                                              ns,
                                              volume_size ?
                                              *volume_size :
                                              default_volume_size(),
                                              new_owner_tag())
                .sco_multiplier(sco_mult ?
                                *sco_mult :
                                default_sco_multiplier())
                .lba_size(lba_size ?
                          *lba_size :
                          default_lba_size())
                .cluster_multiplier(cluster_mult ?
                                    *cluster_mult :
                                    default_cluster_multiplier())
                .metadata_cache_capacity(num_cached_pages)
                .metadata_backend_config(mdstore_test_setup_->make_config()));

    return newVolume(params);
}

SharedVolumePtr
VolManagerTestSetup::newVolume(const VanillaVolumeConfigParameters& params)
{
    VolManager *vm = VolManager::get();

    // make sure the test doesn't mess with existing directories.
    bool meta_dir_created;
    const fs::path meta_dir(vm->getMetaDataPath() / params.get_nspace().str());
    if (fs::exists(meta_dir))
    {
        meta_dir_created = false;
    }
    else
    {
        fs::create_directories(meta_dir);
        meta_dir_created = true;
    }

    bool tlog_dir_created;
    const fs::path tlog_dir(vm->getTLogPath() / params.get_nspace().str());
    if (fs::exists(tlog_dir))
    {
        tlog_dir_created = false;
    }
    else
    {
        fs::create_directories(tlog_dir);
        tlog_dir_created = true;
    }

    try
    {
        (Namespace(params.get_nspace()));
        fungi::ScopedLock m(vm->getLock_());
        SharedVolumePtr v = vm->createNewVolume(params);
        // hello clang analyzer
        VERIFY(v != nullptr);

        return v;
    }
    catch (...)
    {
        if (meta_dir_created)
        {
            fs::remove_all(meta_dir);
        }
        if (tlog_dir_created)
        {
            fs::remove_all(tlog_dir);
        }
        throw;
    }
}

WriteOnlyVolume*
VolManagerTestSetup::newWriteOnlyVolume(const std::string& id,
                                        const be::Namespace& ns,
                                        const VolumeConfig::WanBackupVolumeRole role,
                                        const boost::optional<VolumeSize>& volume_size,
                                        const boost::optional<SCOMultiplier>& sco_mult,
                                        const boost::optional<LBASize>& lba_size,
                                        const boost::optional<ClusterMultiplier>& cluster_mult)
{
    VolManager *vm = VolManager::get();

    // make sure the test doesn't mess with existing directories.
    bool meta_dir_created;
    const fs::path meta_dir(vm->getMetaDataPath() / ns.str());
    if (fs::exists(meta_dir))
    {
        meta_dir_created = false;
    }
    else
    {
        fs::create_directories(meta_dir);
        meta_dir_created = true;
    }

    bool tlog_dir_created;
    const fs::path tlog_dir(vm->getTLogPath() / ns.str());
    if (fs::exists(tlog_dir))
    {
        tlog_dir_created = false;
    }
    else
    {
        fs::create_directories(tlog_dir);
        tlog_dir_created = true;
    }

    try
    {
        // test_namespaces.push_back(make_random_namespace(ns.str()));

        fungi::ScopedLock m(vm->getLock_());

        const auto params = WriteOnlyVolumeConfigParameters(VolumeId(id),
                                                            ns,
                                                            volume_size ?
                                                            *volume_size :
                                                            default_volume_size(),
                                                            role,
                                                            new_owner_tag())
            .sco_multiplier(sco_mult ?
                            *sco_mult :
                            default_sco_multiplier())
            .lba_size(lba_size ?
                      *lba_size :
                      default_lba_size())
            .cluster_multiplier(cluster_mult ?
                                *cluster_mult :
                                default_cluster_multiplier());

        // Push VolumeID out!
        WriteOnlyVolume* v = vm->createNewWriteOnlyVolume(params);

        return v;
    }
    catch (...)
    {
        if (meta_dir_created)
        {
            fs::remove_all(meta_dir);
        }
        if (tlog_dir_created)
        {
            fs::remove_all(tlog_dir);
        }
        throw;
    }
}

SharedVolumePtr
VolManagerTestSetup::localRestart(const be::Namespace& ns,
                                  const FallBackToBackendRestart fallback)
{
    VolManager *vm = VolManager::get();
    fs::create_directories(VolManager::get()->getMetaDataPath() / ns.str());
    fs::create_directories(VolManager::get()->getTLogPath() / ns.str());

    VolumeConfig cfg;
    VolManager::get()->createBackendInterface(ns)->fillObject(cfg,
                                                              VolumeConfig::config_backend_name,
                                                              InsistOnLatestVersion::T);

    fungi::ScopedLock m(vm->getLock_());
    SharedVolumePtr v = vm->local_restart(ns,
                                  cfg.owner_tag_,
                                  fallback,
                                  IgnoreFOCIfUnreachable::F);

    // yo, clang analyzer: this one's especially for you.
    VERIFY(v != nullptr);
    return v;
}

SharedVolumePtr
VolManagerTestSetup::getVolume(const VolumeId &name)
{
    VolManager *vm = VolManager::get();
    SharedVolumePtr v = 0;
    fungi::ScopedLock m(vm->getLock_());
    v= vm->findVolume_noThrow_(name);
    return v;
}

SharedVolumePtr
VolManagerTestSetup::createClone(const be::BackendTestSetup::WithRandomNamespace& wrns,
                                 const be::Namespace& parentns,
                                 const boost::optional<SnapshotName>& snapshot)
{
    return createClone(wrns.ns().str(),
                       wrns.ns(),
                       parentns,
                       snapshot);
}

SharedVolumePtr
VolManagerTestSetup::createClone(const std::string& cloneName,
                                 const be::Namespace& newNamespace,
                                 const be::Namespace& parentns,
                                 const boost::optional<SnapshotName>& parent_snap)
{
    VolManager *vm = VolManager::get();
    fs::create_directories(vm->getMetaDataPath() / newNamespace.str());
    fs::create_directories(vm->getTLogPath() / newNamespace.str());

    SharedVolumePtr clone = 0;

    try
    {
        //        test_namespaces.push_back(make_random_namespace(newNamespace.str()));
        fungi::ScopedLock m(vm->getLock_());
        const auto params(CloneVolumeConfigParameters(VolumeId(cloneName),
                                                      newNamespace,
                                                      parentns,
                                                      new_owner_tag())
                          .parent_snapshot(parent_snap)
                          .metadata_backend_config(mdstore_test_setup_->make_config()));

        clone = vm->createClone(params,
                                PrefetchVolumeData::F);
    }
    catch(...)
    {
        LOG_ERROR("Problem creating clone");

        fs::remove_all(vm->getMetaDataPath() / newNamespace.str());
        fs::remove_all(vm->getTLogPath() / newNamespace.str());
        throw;
    }

    // this one's here for clang analyzer.
    VERIFY(clone != nullptr);
    // Y42 to get better test coverage

    return clone;
}


// grrr naming...
void
VolManagerTestSetup::readVolume_(Volume& volume,
                                 uint64_t lba,
                                 uint64_t block_size,
                                 const std::string* const pattern,
                                 unsigned retries)
{
    EXPECT_EQ(0U, block_size % volume.getLBASize()) << "Fix your test";
    EXPECT_GE(volume.getSize(), lba * volume.getLBASize() + block_size)
        << "Fix your test";

    std::vector<uint8_t> buf(block_size);

    do
    {
        try
        {
            volume.read(lba, &buf[0], buf.size());
            break;
        }
        catch(TransientException& e)
        {
            LOG_DEBUG("caught transient exception " << e.what());
            if (retries-- > 0)
            {
                LOG_DEBUG("retrying");
                boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            }
            else
            {
                LOG_DEBUG("giving up");
                throw;
            }
        }
    }
    while (true);

    if (pattern != 0 and pattern->size() != 0)
    {
        for(uint64_t i = 0; i < buf.size(); ++i)
        {
            ASSERT_TRUE(buf[i] == (*pattern)[i % pattern->size()]);
        }
    }
}

void
VolManagerTestSetup::readVolume(Volume& vol,
                                uint64_t block_size,
                                unsigned retries)
{
    for (size_t i = 0; i < vol.getSize(); i += block_size)
    {
        readVolume_(vol, i / vol.getLBASize(), block_size, 0, retries);
    }
}

void
VolManagerTestSetup::setFailOverCacheConfig(const VolumeId& volid,
                                            const boost::optional<FailOverCacheConfig>& maybe_config)
{
    SharedVolumePtr v;
    ASSERT_NO_THROW(v = VolManager::get()->findVolume_(volid));
    v->setFailOverCacheConfig(maybe_config);
}


void
VolManagerTestSetup::checkVolume(Volume& volume,
                                 uint64_t lba,
                                 uint64_t block_size,
                                 const std::string& pattern,
                                 unsigned retries)
{
    readVolume_(volume, lba, block_size, &pattern, retries);
}

void
VolManagerTestSetup::checkVolume_aux(Volume& v,
                                 const std::string& pattern,
                                 uint64_t size,
                                 uint64_t block_size,
                                 unsigned retries)
{
    for (size_t i = 0; i < size; i += block_size)
    {
        checkVolume(v, i / v.getLBASize(), block_size, pattern, retries);
    }
}

void
VolManagerTestSetup::checkVolume(Volume& v,
                                 const std::string& pattern,
                                 uint64_t block_size,
                                 unsigned retries)
{
    checkVolume_aux(v, pattern, v.getSize(), block_size, retries);
}

bool
VolManagerTestSetup::checkVolumesIdentical(Volume& v1,
                                           Volume& v2) const
{
    const int clustersz = v1.getClusterSize();
    const uint64_t volsz = v1.getSize();
    const int mult = clustersz / v1.getLBASize();

    ASSERT(v1.getSize() == v2.getSize());

    std::vector<uint8_t> buf1(clustersz);
    std::vector<uint8_t> buf2(clustersz);
    for(uint64_t i = 0; i < volsz/clustersz ; ++i)
    {
        v1.read(i*mult, &buf1[0], clustersz);
        v2.read(i*mult, &buf2[0], clustersz);
        if(buf1 != buf2)
        {
            return false;
        }
    }
    return true;
}

void
VolManagerTestSetup::createSnapshot(Volume& v, const std::string &name)
{
    VolManager *vm = VolManager::get();
    fungi::ScopedLock m(vm->getLock_());
    v.createSnapshot(SnapshotName(name));
}

void
VolManagerTestSetup::restoreSnapshot(Volume& v, const std::string &name)
{
    VolManager *vm = VolManager::get();
    fungi::ScopedLock m(vm->getLock_());
    v.restoreSnapshot(SnapshotName(name));
}

void
VolManagerTestSetup::flushFailOverCache(Volume& v)
{
    if (v.failover_.get())
    {
        v.failover_->Flush();
    }
}

FailOverCacheClientInterface*
VolManagerTestSetup::getFailOverWriter(Volume& v)
{
    auto b = v.failover_.get();
    EXPECT_TRUE(b != nullptr);
    return b;
}

SnapshotManagement*
VolManagerTestSetup::getSnapshotManagement(Volume& v)
{
    return v.snapshotManagement_.get();
}

void
VolManagerTestSetup::blockBackendWrites_(VolumeInterface* v)
{
    if(blockers_.find(v) != blockers_.end())
    {
        LOG_WARN("Trying to block the backend for a volume that is already blocked");
        return;
    }
    try
    {
        BSemTask* b = new BSemTask(v);

        VolManager::get()->scheduleTask(b);
        scheduleBarrierTask(*v);
        blockers_[v] = b;
        // Not completely ok!!
        b->waitForRun();

    }
    catch(...)
    {
        LOG_ERROR("Could not initialize semaphore for blocking backend");
        throw;
    }

}

class UnblockInFuture
    : public fungi::Runnable
{
public:
    UnblockInFuture(VolManagerTestSetup* vm,
                    unsigned secs,
                    VolumeInterface* v)
        :vm_(vm),
         secs_(secs),
         v_(v)
    {}

    virtual void
    run()
    {
        sleep(secs_);
        vm_->unblockBackendWrites_(v_);
    }

    virtual const char*
    getName() const
    {
        return "UnblockInFuture";
    }

private:
    VolManagerTestSetup* vm_;
    unsigned secs_;
    VolumeInterface* v_;
};


void
VolManagerTestSetup::unblockBackendWrites_(VolumeInterface* v)
{
    if (blockers_.find(v) == blockers_.end())
    {
        LOG_WARN("Trying to unblock a volume that isn't blocked");
    }
    else
    {
        blockers_[v]->cont();
        blockers_.erase(v);
    }
}

fungi::Thread*
VolManagerTestSetup::futureUnblockBackendWrites(VolumeInterface* v, unsigned secs)
{
    UnblockInFuture* tmp = new UnblockInFuture(this, secs, v);
    fungi::Thread* t = new fungi::Thread(*tmp);
    t->StartNonDetachedWithCleanup();
    return t;
}

const CachedSCOPtr
VolManagerTestSetup::getCurrentSCO(Volume& v)
{
    return v.dataStore_->currentSCO_()->sco_ptr();
}

bool
VolManagerTestSetup::isVolumeSyncedToBackend(Volume& v)
{
    return v.isSyncedToBackend();
}

void
VolManagerTestSetup::scheduleBackendSync(Volume& v)
{
    v.scheduleBackendSync();
}

void
VolManagerTestSetup::writeSnapshotFileToBackend(Volume& v)
{
    v.snapshotManagement_->scheduleWriteSnapshotToBackend();
}

void
VolManagerTestSetup::scheduleBarrierTask(VolumeInterface& v)
{
    VolManager::get()->scheduleTask(new backend_task::Barrier(&v));
}

void
VolManagerTestSetup::scheduleThrowingTask(Volume& v)
{
    VolManager::get()->scheduleTask(new ThrowingTask(&v));
}

class SortSCO
{
public:
    bool
    operator()(const SCO first, const SCO second) const
    {
        return first.number() < second.number();
    }
};

void
VolManagerTestSetup::removeNonDisposableSCOS(Volume& v)
{
    SCONameList lst;
    VolManager::get()->getSCOCache()->getSCONameList(v.getNamespace(),
                                                     lst,
                                                     false);
    SortSCO s;
    lst.sort(s);
    // Y42: Don't remove the current sco otherwise BOOM at some stage.
    if(not lst.empty())
    {
        LOG_INFO("Removing SCO " << lst.back());
        lst.pop_back();
    }


    for(SCONameList::const_iterator it = lst.begin();
        it != lst.end();
        ++it)
    {
        VolManager::get()->getSCOCache()->removeSCO(v.getNamespace(),
                                                    *it,
                                                    true);
    }
}

void
VolManagerTestSetup::persistXVals(const VolumeId& volname) const
{
    fungi::ScopedLock l(VolManager::get()->getLock_());
    SharedVolumePtr v = VolManager::get()->findVolume_(volname);
    if (v != nullptr)
    {
        SCOAccessData sad(v->getNamespace(),
                          v->readActivity());
        VolManager::get()->getSCOCache()->fillSCOAccessData(sad);

        SCOAccessDataPersistor sadp(v->getBackendInterface()->cloneWithNewNamespace(v->getNamespace()));
        sadp.push(sad,
                  v->backend_write_condition());
    }
    else
    {
        // Y42 can we assert here of not?
        LOG_WARN("Volume " << volname << " not found, removed behind our backs?");
    }
}

void
VolManagerTestSetup::waitForPrefetching(Volume& v) const
{
    PrefetchData& pd = v.getPrefetchData();
    while (!pd.scos.empty())
    {
        sleep(1);
    }
    // Y42 Do an extra sleep here because the queue might be empty but there could
    // have been one in the barrel
    sleep(2);
}

void
VolManagerTestSetup::updateReadActivity() const
{
    VolManager* vm = VolManager::get();
    fungi::ScopedLock l(vm->getLock_());
    unsigned count = 0;
    while(not vm->updateReadActivity()
          and (count < 5))
    {
        ++count;
        sleep(1);
    }
    if(count >= 5)
    {
        throw fungi::IOException("Could not update read activity");
    }
}


void
VolManagerTestSetup::setNamespaceRestarting(const Namespace& ns,
                                            const VolumeConfig& volume_config)
{
    fungi::ScopedLock l(api::getManagementMutex());
    VolManager::get()->setNamespaceRestarting_(ns, volume_config);
}

void
VolManagerTestSetup::clearNamespaceRestarting(const Namespace& ns)
{
    fungi::ScopedLock l(api::getManagementMutex());
    size_t res = VolManager::get()->restartMap_.erase(ns);
    EXPECT_EQ(1U, res);
}

void
VolManagerTestSetup::writeVolumeConfigToBackend(Volume& vol) const
{
    return vol.writeConfigToBackend_(vol.get_config());
}

SCOCache::SCOCacheMountPointList&
VolManagerTestSetup::getSCOCacheMountPointList()
{
    return VolManager::get()->getSCOCache()->mountPoints_;
}

SCOCache::NSMap&
VolManagerTestSetup::getSCOCacheNamespaceMap()
{
    return VolManager::get()->getSCOCache()->nsMap_;
}

// setting the volume's namespace in that mountpoint readonly prevents SCOs
// from being removed. unless you're root.
void
VolManagerTestSetup::breakMountPoint(SCOCacheMountPointPtr mp,
                                     const be::Namespace& nspace)
{
    EXPECT_TRUE(geteuid() != 0) << "fix your test";
    fs::path p(mp->getPath() / nspace.str());
    EXPECT_EQ(0, ::chmod(p.string().c_str(), S_IRUSR|S_IXUSR));
}

yt::Weed
VolManagerTestSetup::growWeed()
{
    const uint64_t input_size = 4096;

    std::vector<byte> data(input_size, 0x0f);
    return yt::Weed(data);
}

void
VolManagerTestSetup::syncMetaDataStoreBackend(Volume& v)
{
    auto md = dynamic_cast<CachedMetaDataStore*>(v.metaDataStore_.get());
    if (md != nullptr)
    {
        md->backend_->sync();
    }
    else
    {
        FAIL() << "What type of metadatastore is this?";
    }
}

MetaDataBackendType
VolManagerTestSetup::metadata_backend_type() const
{
    VERIFY(mdstore_test_setup_ != nullptr);
    return mdstore_test_setup_->backend_type_;
}

VolumeDriverTestConfig
VolManagerTestSetup::default_test_config()
{
    static const VolumeDriverTestConfig cfg =
        VolumeDriverTestConfig().use_cluster_cache(true);
    return cfg;
}

void
VolManagerTestSetup::fill_backend_cache(const be::Namespace& ns)
{

    switch(VolManager::get()->getBackendConnectionManager()->config().backend_type.value())
    {
    case be::BackendType::LOCAL:
        {
            auto con = VolManager::get()->getBackendConnectionManager()->getConnection();

            be::BackendConnectionInterface::PartialReads partial_reads;
            {
                fs::path p = FileUtils::create_temp_file_in_temp_dir("backend_temp");
                ALWAYS_CLEANUP_FILE(p);
                const std::string
                    s("arbitrary string to write to an object in the cache");

                FileDescriptor(p, FDMode::Write).write(s.data(),
                                                       s.size());

                std::vector<char> rbuf(s.size());

                for(unsigned i = 0; i < be::local::Connection::lru_cache_size; ++i)
                {
                    const std::string
                        oname(boost::lexical_cast<std::string>(i));
                    con->write(ns,
                               p,
                               oname);

                    be::BackendConnectionInterface::ObjectSlices
                        slices { be::BackendConnectionInterface::ObjectSlice(rbuf.size(),
                                                                             0,
                                                                             reinterpret_cast<uint8_t*>(rbuf.data())) };
                    EXPECT_TRUE(partial_reads.emplace(std::make_pair(oname,
                                                                     std::move(slices))).second);
                }

                const fs::path cache_dir(yt::FileUtils::temp_path() / "partial-read-cache");
                fs::create_directories(cache_dir);
                ALWAYS_CLEANUP_DIRECTORY(cache_dir);

                be::SimpleFetcher fetcher(*con,
                                          ns,
                                          cache_dir);

                con->partial_read(ns,
                                  partial_reads,
                                  InsistOnLatestVersion::T,
                                  fetcher);

                EXPECT_EQ(s,
                          std::string(rbuf.data(),
                                      rbuf.size()));
            }
        }

        break;
    case be::BackendType::S3:
        break;
    case be::BackendType::MULTI:
        break;
    case be::BackendType::ALBA:
        break;
    }
}


void
VolManagerTestSetup::destroyVolume(WriteOnlyVolume* v,
                                   const RemoveVolumeCompletely /*cleanup_backend*/)
{
    Namespace ns(v->getNamespace());

    const std::string name = v->getName();

    VolManager *vm = VolManager::get();

    {
        fungi::ScopedLock m(vm->getLock_());
        vm->destroyVolume(v,
                          RemoveVolumeCompletely::F);
    }

    // Y42 these names must be made somewhere else!
    remove_all(directory_ / name);
    remove_all(directory_ / "metadatastores" / ns.str());
    remove_all(directory_ / "tlogs" / ns.str());
}

void
VolManagerTestSetup::destroyVolume(SharedVolumePtr& v,
                                   const DeleteLocalData delete_local_data,
                                   const RemoveVolumeCompletely remove_volume_completely)
{
    const be::Namespace ns = v->getNamespace();

    const std::string name = v->getName();

    VolManager *vm = VolManager::get();

    {
        fungi::ScopedLock m(vm->getLock_());
        vm->destroyVolume(v,
                          delete_local_data,
                          remove_volume_completely);
    }

    v = nullptr;

    if(T(delete_local_data))
    {
        // Y42 these names must be made somewhere else!
        remove_all(directory_ / name);
        remove_all(directory_ / "metadatastores" / ns.str());
        remove_all(directory_ / "tlogs" / ns.str());
    }
}

OwnerTag
VolManagerTestSetup::new_owner_tag()
{
    static std::atomic<uint64_t> t(1);
    return OwnerTag(t.fetch_add(1));
}

uint64_t
VolManagerTestSetup::volume_potential_sco_cache(const ClusterSize c,
                                                const SCOMultiplier s,
                                                const boost::optional<TLogMultiplier>& t)
{
    return VolManager::get()->volumePotentialSCOCache(c,
                                                      s,
                                                      t);
}

void
VolManagerTestSetup::apply_scrub_reply(Volume& v,
                                       const scrubbing::ScrubReply& scrub_reply,
                                       const ScrubbingCleanup cleanup)
{
    boost::optional<be::Garbage> garbage(v.applyScrubbingWork(scrub_reply,
                                                              cleanup));

    if (garbage)
    {
        std::promise<void> promise;
        std::future<void> future(promise.get_future());

        auto fun([&garbage,
                  &promise,
                  nspace = v.getNamespace()]()
                 {
                     be::GarbageCollectorPtr gc(api::backend_garbage_collector());
                     gc->queue(std::move(*garbage));
                     gc->barrier(nspace).get();
                     promise.set_value();
                 });

        v.wait_for_backend_and_run(std::move(fun));

        future.wait();
    }
}

boost::shared_ptr<be::Condition>
VolManagerTestSetup::claim_namespace(const be::Namespace& nspace,
                                     const OwnerTag owner_tag)
{
    return VolumeFactory::claim_namespace(nspace,
                                          owner_tag);
}

}

// Local Variables: **
// mode: c++ **
// End: **
