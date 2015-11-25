// Copyright 2015 Open vStorage NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "VolManagerTestSetup.h"

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <youtils/DimensionedValue.h>

#include "../Api.h"
#include "../DataStoreNG.h"
#include "../TransientException.h"
#include "../VolManager.h"

namespace volumedriver
{

// These tests disable the read cache so it doesn't paper over errors in
// the scocache. A rename to sth. more specific (DataStoreErrorHandlingTest?) is
// in order.

struct SCOTruncater
{
    explicit SCOTruncater(const backend::Namespace& nspace)
        : nspace_(nspace)
    {}

    void
    operator()(SCONumber sconum) const
    {
        ClusterLocation loc(sconum);
        CachedSCOPtr sco =
            VolManager::get()->getSCOCache()->findSCO(nspace_,
                                                      loc.sco());

        ASSERT_TRUE(sco.get());
        FileUtils::truncate(sco->path(), 42);
    }

    const backend::Namespace nspace_;
};

struct SCORemover
{
    explicit SCORemover(const backend::Namespace& nspace)
        : nspace_(nspace)
    {}

    void
    operator()(SCONumber sconum) const
    {
        ClusterLocation loc(sconum);
        CachedSCOPtr sco =
            VolManager::get()->getSCOCache()->findSCO(nspace_,
                                                      loc.sco());

        ASSERT_TRUE(sco.get());
        fs::remove(sco->path());
    }

    const backend::Namespace nspace_;
};

class ErrorHandlingTest
    : public VolManagerTestSetup
{
protected:
    static const SCOMultiplier sco_mult_;
    static constexpr size_t lba_size_ = 512;
    static constexpr size_t cluster_mult_ = 8;
    static const size_t sco_size_;

    static const size_t mp_size_;
    static const size_t trigger_gap_;
    static const size_t backoff_gap_;

    static std::string
    bytesToString(size_t bytes)
    {
        std::stringstream ss;
        ss << bytes << "B";
        return ss.str();
    }

public:
    ErrorHandlingTest()
        : VolManagerTestSetup("ErrorHandlingTest",
                              UseFawltyMDStores::F,
                              UseFawltyTLogStores::F,
                              UseFawltyDataStores::F,
                              4, // num backend threads
                              bytesToString(mp_size_), // scocache mp1 size
                              bytesToString(mp_size_), // scocache mp2 size
                              bytesToString(trigger_gap_),
                              bytesToString(backoff_gap_),
                              3600, // clean interval
                              32, // open scos per volume
                              4000, // datastore throttle usecs
                              10000, // foc throttle usecs
                              6) // num scos / tlog
        , volname_("volume")
        , vol_(0)
    {}

    virtual void
    SetUp()
    {
        cleanup();

        VolManagerTestSetup::SetUp();
        volns_ptr_ = make_random_namespace(volns_);

        vol_ = newVolume(volname_,
                         volns_,
                         VolumeSize((1 << 20) * 512),
                         sco_mult_,
                         lba_size_,
                         cluster_mult_);

        fungi::ScopedLock l(api::getManagementMutex());
    }

    virtual void
    TearDown()
    {
        if (foc_ctx_ != nullptr)
        {
            stopFOC();
        }

        if(vol_)
        {
            destroyVolume(vol_,
                          DeleteLocalData::T,
                          RemoveVolumeCompletely::T);
        }
        volns_ptr_.reset();

        cleanup();
        VolManagerTestSetup::TearDown();
    }

    void
    cleanup()
    {
        std::list<fs::path> paths;

        paths.push_back(sc_mp1_path_ / static_cast<const std::string&>(volns_.str()));
        paths.push_back(sc_mp2_path_ / static_cast<const std::string&>(volns_.str()));

        BOOST_FOREACH(fs::path& p, paths)
        {
            if (fs::exists(p))
            {
                EXPECT_EQ(0, ::chmod(p.string().c_str(),
                                     S_IWUSR|S_IRUSR|S_IXUSR));
            }
        }
    }

    SCOCache::SCOCacheMountPointList&
    getMountPointList()
    {
        return VolManager::get()->getSCOCache()->mountPoints_;
    }

    void
    startFOC()
    {
        foc_ctx_ = start_one_foc();
        vol_->setFailOverCacheConfig(foc_ctx_->config());
    }

    void
    stopFOC()
    {
        try
        {
            vol_->setFailOverCacheConfig(boost::none);
        }
        catch (std::exception&)
        {
            // the test might have offlined the last mountpoint so it might not
            // be possible to modify the volume's settings -- swallow the exception
        }

        foc_ctx_.reset();
    }

    void
    breakCurrentSCO()
    {
        DataStoreNG* ds = vol_->getDataStore();
        CachedSCOPtr sco = ds->currentSCO_()->sco_ptr();
        ASSERT(sco != 0);

        OpenSCOPtr osco(sco->open(FDMode::Read));
        ds->openSCOs_.insert(osco);

        // writes will fail
    }

    void
    writeTest(bool with_foc)
    {
        EXPECT_EQ(2U, getMountPointList().size());

        const std::string pattern("abcd");
        const size_t size = vol_->getClusterSize();

        EXPECT_TRUE(size % pattern.size() == 0) << "fix your test";

        if (with_foc)
        {
            startFOC();
        }

        writeToVolume(vol_,
                      0,
                      size,
                      pattern);

        breakCurrentSCO();

        uint64_t off = size / vol_->getLBASize();
        unsigned cnt = 1;

        if (with_foc)
        {
            writeToVolume(vol_,
                          off,
                          size,
                          pattern);

            checkVolume(vol_,
                        0,
                        2 * size,
                        pattern);

            EXPECT_EQ(1U, getMountPointList().size());

            breakCurrentSCO();

            ++cnt;
        }

        EXPECT_THROW(writeToVolume(vol_,
                                   (size * cnt) / vol_->getLBASize(),
                                   size,
                                   pattern),
                     std::exception);

        EXPECT_THROW(checkVolume(vol_,
                                 0,
                                 size * cnt,
                                 pattern),
                     std::exception);

        if (with_foc)
        {
            EXPECT_EQ(0U, getMountPointList().size());
        }
        else
        {
            EXPECT_EQ(1U, getMountPointList().size());
        }

        EXPECT_TRUE(vol_->is_halted());
    }

    template<typename T> void
    readTest(T& scocorrupter,
             bool block,
             bool with_foc,
             uint64_t size,
             SCONumber sco_to_break)
    {
        EXPECT_EQ(2U, getMountPointList().size());

        std::unique_ptr<ScopedBackendBlocker> blocker;
        if (block)
        {
            blocker.reset(new ScopedBackendBlocker(this, vol_));
        }

        if (with_foc)
        {
            startFOC();
        }

        const std::string pattern("abc");

        writeToVolume(vol_,
                      0,
                      size,
                      pattern);

        if (!block)
        {
            syncToBackend(vol_);
        }

        // we need to sync() here, otherwise our / the OS'es caching mechanisms
        // could interfere
        vol_->sync();

        bool disposable(true);

        {
            ClusterLocation loc(sco_to_break);
            SCOCache* c = VolManager::get()->getSCOCache();
            CachedSCOPtr sco = c->findSCO(volns_,
                                          loc.sco());
            ASSERT_TRUE(sco != 0);
            disposable = c->isSCODisposable(sco);
        }

        scocorrupter(sco_to_break);

        if (with_foc || disposable)
        {
            checkVolume(vol_,
                        0,
                        size,
                        pattern);

            EXPECT_EQ(1U, getMountPointList().size());

            scocorrupter(sco_to_break);

            EXPECT_THROW(checkVolume(vol_,
                                     0,
                                     vol_->getClusterSize(),
                                     pattern),
                         std::exception);

            EXPECT_EQ(0U, getMountPointList().size());

            EXPECT_THROW(writeToVolume(vol_,
                                       0,
                                       vol_->getClusterSize(),
                                       pattern),
                         std::exception) <<
                "I still can write though all mountpoints are gone!";
        }
        else
        {
            EXPECT_THROW(checkVolume(vol_,
                                     0,
                                     size,
                                     pattern),
                         std::exception);

            EXPECT_EQ(1U, getMountPointList().size());
        }

        EXPECT_TRUE(vol_->is_halted());
    }

    void
    backendPutTest(bool with_foc)
    {
        EXPECT_EQ(2U, getMountPointList().size());

        const size_t size = sco_size_ * 2 - lba_size_ * cluster_mult_;

        std::unique_ptr<ScopedBackendBlocker>
            blocker(new ScopedBackendBlocker(this, vol_));

        if (with_foc)
        {
            startFOC();
        }

        std::string pattern("abcd");

        writeToVolume(vol_,
                      0,
                      size,
                      pattern);

        vol_->sync();

        SCONameList scos;
        SCOCache* cache = VolManager::get()->getSCOCache();
        cache->getSCONameList(volns_, scos, false);

        EXPECT_EQ(2U, scos.size());
        EXPECT_LT(1U,
                  vol_->getDataStore()->currentSCO_()->sco_ptr()->getSCO().number());

        scos.clear();
        cache->getSCONameList(volns_, scos, true);
        EXPECT_EQ(0U, scos.size());

        ClusterLocation loc(1);
        SCOTruncater truncater(volns_);
        truncater(loc.number());

        if (with_foc)
        {
            blocker.reset(0);

            waitForThisBackendWrite(vol_);

            EXPECT_EQ(1U, getMountPointList().size());

            syncToBackend(vol_);

            cache->getSCONameList(volns_, scos, true);
            EXPECT_EQ(2U, scos.size());
            EXPECT_TRUE(loc.sco() == scos.front() ||
                        loc.sco() == scos.back());

            blocker.reset(new ScopedBackendBlocker(this, vol_));

            checkVolume(vol_,
                        0,
                        size,
                        pattern);

            pattern = "efgh";

            writeToVolume(vol_,
                          0,
                          size,
                          pattern);

            vol_->sync();

            loc = ClusterLocation(3);
            EXPECT_LT(loc.number(),
                      vol_->getDataStore()->currentSCO_()->sco_ptr()->getSCO().number());

            truncater(loc.number());
        }

        blocker.reset(0);

        // wait for the backend writer to run into the error
        sleep(10);

        EXPECT_THROW(checkVolume(vol_,
                                 0,
                                 size,
                                 pattern),
                     std::exception);

        if (with_foc)
        {
            EXPECT_EQ(0U, getMountPointList().size());
        }
        else
        {
            EXPECT_EQ(1U, getMountPointList().size());
        }

        EXPECT_TRUE(vol_->is_halted());
    }

    void
    fetcherTest(bool with_foc)
    {
        EXPECT_EQ(2U, getMountPointList().size());

        const std::string pattern("efgh");
        const size_t size = vol_->getClusterSize() * (vol_->getSCOMultiplier() + 1);

        EXPECT_TRUE(size % pattern.size() == 0) << "fix your test";

        std::unique_ptr<ScopedBackendBlocker> blocker;
        if (with_foc)
        {
            startFOC();
            blocker.reset(new ScopedBackendBlocker(this, vol_));
        }

        writeToVolume(vol_,
                      0,
                      size,
                      pattern);

        if (with_foc)
        {
            // create a new sco - the snap will not make it to the backend
            createSnapshot(vol_, "snap");
        }
        else
        {
            syncToBackend(vol_);
        }

        // remove it from the SCOCache, break the mountpoint it will be fetched to
        // and try to read again. reading should not fail but the mountpoint needs
        // to be offlined.
        // this is rather brittle: the test obviously needs to make sure that the
        // mountpoint balancing will chose the correct mountpoint to fetch the
        // sco to.
        ClusterLocation loc(2);
        removeSCOAndBreakMountPoint_(loc.sco());

        EXPECT_NO_THROW(checkVolume(vol_,
                                    0,
                                    size,
                                    pattern));

        EXPECT_EQ(1U, getMountPointList().size());
        ASSERT_NO_THROW(writeToVolume(vol_,
                                      size / vol_->getLBASize(),
                                      size,
                                      pattern));

        if (!with_foc)
        {
            syncToBackend(vol_);
        }

        removeSCOAndBreakMountPoint_(loc.sco());

        EXPECT_THROW(checkVolume(vol_,
                                 0,
                                 size,
                                 pattern),
                     std::exception);

        EXPECT_EQ(0U, getMountPointList().size());

        EXPECT_THROW(writeToVolume(vol_,
                                   0,
                                   size,
                                   pattern),
                     std::exception) <<
            "I still can write though all mountpoints are gone!";

        EXPECT_TRUE(vol_->is_halted());
    }

    void
    checkHalted()
    {
        std::vector<byte> buf(vol_->getClusterSize());
        memset(&buf[0], 0xff, buf.size());

        EXPECT_TRUE(vol_->is_halted());

        ASSERT_THROW(vol_->checkNotHalted_(),
                     std::exception);

        ASSERT_THROW(vol_->read(0, &buf[0], buf.size()),
                     std::exception);
        ASSERT_THROW(vol_->write(0, &buf[0], buf.size()),
                     std::exception);
        ASSERT_THROW(vol_->scheduleBackendSync(),
                     std::exception);
        ASSERT_THROW(vol_->sync(),
                     std::exception);

        const std::string snap2("snap2");
        ASSERT_THROW(vol_->createSnapshot(snap2),
                     std::exception);

        std::list<std::string> snaps;
        vol_->listSnapshots(snaps);

        ASSERT_THROW(vol_->setFOCTimeout(42),
                     std::exception);

        ASSERT_THROW(vol_->setFailOverCacheConfig(boost::none),
                     std::exception);

        ASSERT_THROW(vol_->setFailOverCacheConfig(foc_ctx_->config()),
                     std::exception);

        // this one's fishy, since there's no proof that it throws because it's halted
        // -- it could also throw b/c the scrubres does not exist
        const std::string scrubres("scrubres");
        ASSERT_THROW(vol_->applyScrubbingWork(scrubres),
                     std::exception);

        ASSERT_THROW(vol_->checkConsistency(),
                     std::exception);
    }

protected:
    const VolumeId volname_;
    std::unique_ptr<WithRandomNamespace> volns_ptr_;

    const backend::Namespace volns_;
    Volume* vol_;
    volumedrivertest::foctest_context_ptr foc_ctx_;

private:

    void
    removeSCOAndBreakMountPoint_(SCO sconame)
    {
        // sync and close any open write sco's but the current sco
        vol_->sync();

        SCOCache* c = VolManager::get()->getSCOCache();
        SCOCacheMountPointPtr mp;
        {
            CachedSCOPtr sco;
            ASSERT_NO_THROW(sco = c->findSCO(volns_,
                                             sconame));
            //            ASSERT_TRUE(sco);

            ASSERT_TRUE(sco != vol_->getDataStore()->currentSCO_()->sco_ptr());

            mp = sco->getMountPoint();
            c->removeSCO(volns_, sconame, true);
        }

        ASSERT_TRUE(nullptr == c->findSCO(volns_,
                                          sconame));
        ASSERT_TRUE(mp != 0);

        breakMountPoint(mp, vol_->getNamespace());
    }
};

const SCOMultiplier ErrorHandlingTest::sco_mult_(32);

const size_t ErrorHandlingTest::sco_size_ =
    ErrorHandlingTest::lba_size_ *
    ErrorHandlingTest::cluster_mult_ *
    ErrorHandlingTest::sco_mult_;

const size_t ErrorHandlingTest::mp_size_ = ErrorHandlingTest::sco_size_ * 10;
const size_t ErrorHandlingTest::trigger_gap_ = ErrorHandlingTest::sco_size_ * 2;
const size_t ErrorHandlingTest::backoff_gap_ = ErrorHandlingTest::sco_size_ * 7;

TEST_P(ErrorHandlingTest, DISABLED_readTruncatedDisposable)
{
    SCOTruncater truncater(volns_);
    readTest<SCOTruncater>(truncater,
                           false,
                           false,
                           2 * sco_size_,
                           1);
}

TEST_P(ErrorHandlingTest, DISABLED_readRemovedDisposable)
{
    SCORemover remover(volns_);
    readTest<SCORemover>(remover,
                         false,
                         false,
                         2 * sco_size_,
                         1);
}

TEST_P(ErrorHandlingTest, readTruncatedNonDisposableNoFOC)
{
    SCOTruncater truncater(volns_);
    readTest<SCOTruncater>(truncater,
                           true,
                           false,
                           2 * sco_size_,
                           1);
}

TEST_P(ErrorHandlingTest, readRemovedNonDisposableNoFOC)
{
    SCORemover remover(volns_);
    readTest<SCORemover>(remover,
                         true,
                         false,
                         2 * sco_size_,
                         1);
}

TEST_P(ErrorHandlingTest, readTruncatedNonDisposableWithFOC)
{
    SCOTruncater truncater(volns_);
    readTest<SCOTruncater>(truncater,
                           true,
                           true,
                           2 * sco_size_,
                           1);
}

TEST_P(ErrorHandlingTest, readRemovedNonDisposableWithFOC)
{
    SCORemover remover(volns_);
    readTest<SCORemover>(remover,
                         true,
                         true,
                         2 * sco_size_,
                         1);
}

TEST_P(ErrorHandlingTest, readTruncatedCurrentNoFOC)
{
    SCOTruncater truncater(volns_);
    readTest<SCOTruncater>(truncater,
                           true,
                           false,
                           sco_size_ - vol_->getClusterSize(),
                           1);
}

TEST_P(ErrorHandlingTest, readTruncatedCurrentWithFOC)
{
    SCOTruncater truncater(volns_);
    readTest<SCOTruncater>(truncater,
                           true,
                           true,
                           sco_size_ - vol_->getClusterSize(),
                           1);
}

TEST_P(ErrorHandlingTest, writeErrorNoFOC)
{
    writeTest(false);
}

TEST_P(ErrorHandlingTest, writeErrorWithFOC)
{
    writeTest(true);
}

TEST_P(ErrorHandlingTest, cleanupError)
{
    if(::geteuid() == 0)
    {
        return;
    }

    // ASSERT_TRUE(::geteuid() != 0) <<
    //     "this test will fail if run with root permissions";

    size_t mp_count = getMountPointList().size();
    EXPECT_EQ(2U, mp_count);

    const std::string pattern("wxyz");
    size_t size = 2 * mp_size_ - (2 * sco_size_);

    writeToVolume(vol_,
                  0,
                  size,
                  pattern);

    syncToBackend(vol_);

    SCONameList disposables;
    SCOCache* cache = VolManager::get()->getSCOCache();
    cache->getSCONameList(volns_,
                          disposables,
                          true);

    SCONameList broken;
    SCOCacheMountPointPtr mp = getMountPointList().front();

    BOOST_FOREACH(SCO sconame, disposables)
    {
        CachedSCOPtr sco(cache->findSCO(volns_,
                                        sconame));
        if (sco->getMountPoint() == mp)
        {
            broken.push_back(sconame);
        }
    }

    breakMountPoint(mp, vol_->getNamespace());

    cache->cleanup();

    EXPECT_EQ(1U, getMountPointList().size());
    EXPECT_TRUE(mp != getMountPointList().front());

    BOOST_FOREACH(SCO sconame, broken)
    {
        EXPECT_TRUE(nullptr == cache->findSCO(volns_,
                                              sconame));
    }

    // do another cleanup, since the previous one will have bailed out (fix that?)
    // once it encountered an error.
    cache->cleanup();

    for (size_t i = 0; i < size; i += sco_size_)
    {
        checkVolume(vol_,
                    i % lba_size_,
                    sco_size_,
                    pattern);
        cache->cleanup();
    }

    mp = getMountPointList().front();
    size = mp_size_ - mp->getUsedSize() - sco_size_;
    size = (size / sco_size_) * sco_size_;

    writeToVolume(vol_,
                  0,
                  size,
                  pattern);

    syncToBackend(vol_);

    SCONameList scos;

    cache->getSCONameList(volns_, scos, true);
    cache->getSCONameList(volns_, scos, false);

    BOOST_FOREACH(SCO sconame, scos)
    {
        CachedSCOPtr sco(cache->findSCO(volns_,
                                        sconame));
        ASSERT_EQ(mp, sco->getMountPoint());
    }

    breakMountPoint(mp, vol_->getNamespace());

    cache->cleanup();

    scos.clear();
    cache->getSCONameList(volns_, scos, true);
    EXPECT_EQ(0U, scos.size());

    cache->getSCONameList(volns_, scos, false);
    EXPECT_EQ(0U, scos.size());

    EXPECT_EQ(0U, getMountPointList().size());

    //    TODO("No joy here anymore with the partial_reads")
    // EXPECT_THROW(checkVolume(vol_,
    //                          0,
    //                          lba_size_ * cluster_mult_,
    //                          pattern),
    //              std::exception);

    EXPECT_THROW(writeToVolume(vol_,
                               0,
                               lba_size_ * cluster_mult_,
                               pattern),
                 std::exception);

    EXPECT_TRUE(vol_->is_halted());
}

TEST_P(ErrorHandlingTest, backendPutNoFOC)
{
    backendPutTest(false);
}

TEST_P(ErrorHandlingTest, backendPutWithFOC)
{
    backendPutTest(true);
}

TEST_P(ErrorHandlingTest, haltedVolume)
{
    startFOC();

    const size_t size = sco_size_ / 2;
    writeToVolume(vol_,
                  0,
                  size,
                  "blah");

    const std::string snap1("snap1");
    vol_->createSnapshot(snap1);

    waitForThisBackendWrite(vol_);

    EXPECT_FALSE(vol_->is_halted());

    event_collector_->clear();

    vol_->halt();

    checkHalted();

    const boost::optional<events::Event> maybe_ev(event_collector_->pop());
    ASSERT_TRUE(boost::none != maybe_ev);

    ASSERT_TRUE(maybe_ev->HasExtension(events::volumedriver_error));

    const events::VolumeDriverErrorEvent&
        err(maybe_ev->GetExtension(events::volumedriver_error));

    ASSERT_TRUE(err.has_code());
    ASSERT_EQ(events::VolumeDriverErrorCode::VolumeHalted,
              err.code());
    ASSERT_TRUE(err.has_volume_name());
    EXPECT_EQ(volname_.str(),
              err.volume_name());

    const std::string snap2("snap2");
    EXPECT_THROW(vol_->createSnapshot(snap2),
                 std::exception);

    std::list<std::string> snaps;
    vol_->listSnapshots(snaps);

    ASSERT_EQ(1U, snaps.size());
    ASSERT_EQ(snap1, snaps.front());

    ASSERT_THROW(vol_->restoreSnapshot(snap1),
                 std::exception);

    ASSERT_THROW(vol_->deleteSnapshot(snap1),
                 std::exception);
}

// Z42: move this to a better place?
TEST_P(ErrorHandlingTest, reportOfflineMountPoint)
{
    event_collector_->clear();

    SCOCache* c = VolManager::get()->getSCOCache();
    ClusterLocation loc(1);
    CachedSCOPtr sco = c->findSCO(volns_,
                                  loc.sco());

    ASSERT_EQ(0U, event_collector_->size());

    c->reportIOError(sco);

    ASSERT_EQ(1U, event_collector_->size());

    const boost::optional<events::Event> maybe_ev(event_collector_->pop());
    ASSERT_TRUE(boost::none != maybe_ev);

    ASSERT_TRUE(maybe_ev->HasExtension(events::volumedriver_error));

    const events::VolumeDriverErrorEvent&
        err(maybe_ev->GetExtension(events::volumedriver_error));

    ASSERT_TRUE(err.has_code());
    ASSERT_EQ(events::VolumeDriverErrorCode::SCOCacheMountPointOfflined,
              err.code());
}

// WITH PARTIAL READS NO MORE FETCHING OF SCOS
TEST_P(ErrorHandlingTest, DISABLED_fetchToFaultyMountPointFromBackend)
{
    fetcherTest(false);
}

TEST_P(ErrorHandlingTest, DISABLED_fetchToFaultyMountPointFromFOC)
{
    fetcherTest(true);
}

static const VolumeDriverTestConfig a_config(false);

INSTANTIATE_TEST_CASE_P(ErrorHandlingTests,
                        ErrorHandlingTest,
                        ::testing::Values(a_config));
}

// Local Variables: **
// mode: c++ **
// End: **
