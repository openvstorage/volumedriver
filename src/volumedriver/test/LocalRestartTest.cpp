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

#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/scope_exit.hpp>
#include <boost/functional.hpp>

#include <youtils/Assert.h>

#include <volumedriver/DataStoreNG.h>
#include <volumedriver/CachedMetaDataPage.h>
#include <volumedriver/MetaDataStoreInterface.h>
#include <volumedriver/TLogReaderUtils.h>
#include <volumedriver/TLogWriter.h>
#include <volumedriver/VolManager.h>
#include <volumedriver/LocalTLogScanner.h>

namespace volumedriver
{

namespace
{

void
FuckUpSCO(const fs::path& sco,
          const uint64_t offset = 0)
{
    CheckSum s = FileUtils::calculate_checksum(sco);
    ASSERT_TRUE(offset < fs::file_size(sco));

    youtils::FileDescriptor io(sco,
                      FDMode::Write);
    io.seek(offset, Whence::SeekSet);
    for(unsigned char i = 0; i < 255; ++i)
    {
        io.write(&i,1);
        io.sync();
        if(s !=  FileUtils::calculate_checksum(sco))
        {
            return;
        }
    }
    ASSERT_TRUE(false);
}

}

class LocalRestartTest
    : public VolManagerTestSetup
{
protected:
    static const unsigned scos_per_tlog = 2;

public:
    LocalRestartTest()
        : VolManagerTestSetup("LocalRestartTest",
                              UseFawltyMDStores::F,
                              UseFawltyTLogStores::F,
                              UseFawltyDataStores::F,
                              4, // num_threads - default
                              "1GiB", // scocache mp #1 - default
                              "1GiB", // scocache mp #2 - default
                              "250MiB", // scocache trigger gap - default
                              "500MiB", // scocache trigger gap - default
                              60, // scocache cleanup interval - default
                              32, // datastore open sco's per volume - default
                              4000, // datastore throttle usecs - default
                              1000, // foc throttle usecs - default
                              scos_per_tlog) // *not* default
    {}

    // write two tlogs worth of data - the first one goes to the backend while the
    // second one will remain local (path to it is returned)
    fs::path
    seq_write_and_drop_mdstore_cache_and_destroy(Volume *v,
                                                 const std::string& tlog_1_pattern,
                                                 const std::string& tlog_2_pattern)
    {
        const auto max_tlog_entries = v->getSnapshotManagement().maxTLogEntries();
        const auto cluster_size = v->getClusterSize();
        const auto cluster_mult = v->getClusterMultiplier();

        TLogId tlog1;

        {
            OrderedTLogIds tlogs;
            v->getSnapshotManagement().getTLogsNotWrittenToBackend(tlogs);
            EXPECT_EQ(1U, tlogs.size());
            tlog1 = tlogs.front();
        }

        writeToVolume(v,
                      0,
                      max_tlog_entries * cluster_size,
                      tlog_1_pattern);

        waitForThisBackendWrite(v);

        {
            OrderedTLogIds tlogs;
            v->getSnapshotManagement().getTLogsWrittenToBackend(tlogs);
            EXPECT_EQ(1U, tlogs.size());
            EXPECT_EQ(tlog1, tlogs.front());
        }

        TLogId tlog2;

        {
            OrderedTLogIds tlogs;
            v->getSnapshotManagement().getTLogsNotWrittenToBackend(tlogs);
            EXPECT_EQ(1U, tlogs.size());
            tlog2 = tlogs.front();
        }

        EXPECT_NE(tlog1, tlog2);

        const fs::path tlog_path(VolManager::get()->getTLogPath(v) /
                                 boost::lexical_cast<std::string>(tlog2));

        {
            SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                                  DeleteLocalData::F,
                                                  RemoveVolumeCompletely::F);
            writeToVolume(v,
                          max_tlog_entries * cluster_mult,
                          (max_tlog_entries - 1) * cluster_size,
                          tlog_2_pattern);

            // As of VOLDRV-1015 there's no need to drop the MDStore cache explicitly
            // anymore, as the corking mechanism helps our cause
            // dropMetaDataStoreCache(*v);
        }

        return tlog_path;
    }

    void
    check_backend_tlogs(Volume* v, uint32_t expected)
    {
        OrderedTLogIds out;
        v->getSnapshotManagement().getTLogsWrittenToBackend(out);
        std::list<std::string> tlognames;

        for (const auto& tlog_id : out)
        {
            tlognames.push_back(boost::lexical_cast<std::string>(tlog_id));
        }

        tlognames.sort();

        auto bi = v->getBackendInterface()->clone();
        std::list<std::string> tlogs_on_backend;
        bi->listObjects(tlogs_on_backend);
        tlogs_on_backend.remove_if(boost::not1(TLog::isTLogString));
        tlogs_on_backend.sort();

        ASSERT_EQ(expected,
                  tlognames.size());
        ASSERT_TRUE(tlognames == tlogs_on_backend);
    }

    //create a volume, write num_tlogs_on_backend, create a snapshot X, write num_tlogs_on_backend + some data to the current tlog
    //make a clone from X, write num_tlogs_on_backend + some data to the current tlog
    void
    prepare_sanity_check_tests(const uint32_t num_tlogs_on_backend,
                               Volume*& out_parent,
                               Volume*& out_clone,
                               std::unique_ptr<WithRandomNamespace>& parent_ns,
                               std::unique_ptr<WithRandomNamespace>& clone_ns)
    {
        const std::string parent_volname("sanityCheckOfBackendParent");
        const backend::Namespace parent_namespace;
        parent_ns = make_random_namespace(parent_namespace);


        out_parent = newVolume(parent_volname,
                               parent_namespace);

        const auto max_tlog_entries = out_parent->getSnapshotManagement().maxTLogEntries();

        writeClusters(out_parent, num_tlogs_on_backend  * max_tlog_entries - 1);
        std::string thesnap("X");
        createSnapshot(out_parent, thesnap);
        writeClusters(out_parent, (num_tlogs_on_backend + 1) * max_tlog_entries - 1);
        waitForThisBackendWrite(out_parent);

        check_backend_tlogs(out_parent, 2 * num_tlogs_on_backend);

        const std::string volname("sanityCheckOfBackendClone");
        const backend::Namespace ns;
        clone_ns = make_random_namespace(ns);
        out_clone = createClone(volname, ns, parent_namespace, thesnap);
        writeClusters(out_clone, (num_tlogs_on_backend + 1) * max_tlog_entries - 1);
        waitForThisBackendWrite(out_clone);

        check_backend_tlogs(out_clone, num_tlogs_on_backend);
    }

    enum class FOCMode
    {
        None,
        Healthy,
        Broken
    };

    void
    fun_with_half_a_sco(FOCMode focmode)
    {
        // const backend::Namespace ns;
        auto ns_ptr = make_random_namespace();

        const backend::Namespace& ns = ns_ptr->ns();

        Volume* v = newVolume("vol1",
                              ns);

        auto foc_ctx(focmode != FOCMode::None ?
                     start_one_foc() :
                     nullptr);

        if (focmode != FOCMode::None)
        {
            ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));
        }

        const uint64_t cluster_size = v->getClusterSize();
        const uint32_t sco_mult = v->getSCOMultiplier();

        ASSERT_LE(2U, sco_mult);

        const std::string pattern1("bart");
        const std::string pattern2("immanuel");

        MetaDataStoreStats mds1;

        {
            SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                                  2,
                                                  DeleteLocalData::F,
                                                  RemoveVolumeCompletely::F);
            for(unsigned i = 0; i <  sco_mult; ++i)
            {
                writeToVolume(v, 0, cluster_size, pattern1);
            }

            // SCO rollover adds a CRC

            for(unsigned i = 0; i < 2 * sco_mult - 1; ++i)
            {
                writeToVolume(v, 0, cluster_size, pattern2);
            }
            v->getMetaDataStore()->getStats(mds1);
            // EXPECT_LT(0, mds1.used_clusters);

            if (focmode == FOCMode::Broken)
            {
                foc_ctx.reset();
            }
        }

        v = 0;

        // mess up the second SCO
        SCOCache* sc = VolManager::get()->getSCOCache();
        SCOAccessData sad(ns);

        sc->enableNamespace(ns,
                            0,
                            100,
                            sad);
        CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                           ClusterLocation(2).sco());

        ASSERT_TRUE(sco_ptr.get());

        FuckUpSCO(sco_ptr->path(),
                  sco_mult / 2 * cluster_size);

        sc->disableNamespace(ns);

        ASSERT_NO_THROW(v = localRestart(ns));
        ASSERT_TRUE(v);
        //    EXPECT_TRUE(v->mdstore_was_rebuilt_);

        checkVolume(v,
                    0,
                    cluster_size,
                    focmode == FOCMode::Healthy ?
                    pattern2 :
                    pattern1);

        MetaDataStoreStats mds2;
        v->getMetaDataStore()->getStats(mds2);
        // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

        checkCurrentBackendSize(v);
    }
};

TEST_P(LocalRestartTest, normalRestart)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    // const backend::Namespace ns;

    Volume* v1 = newVolume("volume1",
                           ns);
    for(int i = 0; i < 1024; i++)
    {
        writeToVolume(v1,
                      0,
                      v1->getClusterSize(),
                      "kristafke");
    }

    destroyVolume(v1,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v1 = 0;

    ASSERT_NO_THROW(v1 = localRestart(ns));
    ASSERT_TRUE(v1);
    // EXPECT_FALSE(v1->mdstore_was_rebuilt_);

    checkVolume(v1, 0, v1->getClusterSize(), "kristafke");
    checkCurrentBackendSize(v1);

    ASSERT_FALSE(v1->get_cluster_cache_behaviour());
}

TEST_P(LocalRestartTest, readCacheRestart1)
{
    //    const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    Volume* v1 = newVolume("volume1",
                           ns);
    boost::optional<ClusterCacheBehaviour> behaviour(ClusterCacheBehaviour::CacheOnWrite);
    v1->set_cluster_cache_behaviour(behaviour);

    for(int i = 0; i < 1024; i++)
    {
        writeToVolume(v1,
                      0,
                      v1->getClusterSize(),
                      "kristafke");
    }

    destroyVolume(v1,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v1 = 0;

    ASSERT_NO_THROW(v1 = localRestart(ns));
    ASSERT_TRUE(v1);
    // EXPECT_FALSE(v1->mdstore_was_rebuilt_);

    checkVolume(v1, 0, v1->getClusterSize(), "kristafke");
    checkCurrentBackendSize(v1);

    ASSERT_TRUE(*v1->get_cluster_cache_behaviour() == behaviour);
}

TEST_P(LocalRestartTest, normalRestart2)
{
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = newVolume("volume1",
                           ns);
    boost::optional<ClusterCacheBehaviour> behaviour(ClusterCacheBehaviour::NoCache);
    v1->set_cluster_cache_behaviour(behaviour);

    for(int i = 0; i < 1024; i++)
    {
        writeToVolume(v1,
                      0,
                      v1->getClusterSize(),
                      "kristafke");
    }

    destroyVolume(v1,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v1 = 0;

    ASSERT_NO_THROW(v1 = localRestart(ns));
    ASSERT_TRUE(v1);

    // EXPECT_FALSE(v1->mdstore_was_rebuilt_);

    checkVolume(v1, 0, v1->getClusterSize(), "kristafke");
    checkCurrentBackendSize(v1);

    ASSERT_TRUE(*v1->get_cluster_cache_behaviour() == behaviour);
}

TEST_P(LocalRestartTest, normalRestartwithPrefetch)
{
    // const backend::Namespace ns;

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    VolumeId vid("volume1");
    Volume* v1 = newVolume(vid,
                           ns);
    for(int i = 0; i < 1024; i++)
    {
        writeToVolume(v1,
                      0,
                      v1->getClusterSize(),
                      "kristafke");
    }

    ASSERT_NO_THROW(updateReadActivity());
    persistXVals(vid);
    waitForThisBackendWrite(v1);
    destroyVolume(v1,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v1 = 0;

    ASSERT_NO_THROW(v1 = localRestart(ns));
    ASSERT_TRUE(v1);

    //  EXPECT_FALSE(v1->mdstore_was_rebuilt_);

    v1->startPrefetch();
    sleep(1);
    checkVolume(v1, 0, v1->getClusterSize(), "kristafke");
    checkCurrentBackendSize(v1);

}

TEST_P(LocalRestartTest, restartNothing)
{
    // const backend::Namespace ns;

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = newVolume("volume1",
                           ns);

    destroyVolume(v1,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v1 = 0;

    ASSERT_NO_THROW(v1 = localRestart(ns));

    //    EXPECT_FALSE(v1->mdstore_was_rebuilt_);

    checkVolume(v1, 0, v1->getClusterSize(), "\0");
    checkCurrentBackendSize(v1);

}

TEST_P(LocalRestartTest, restartNothingWithBackendSync)
{

    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    Volume* v1 = newVolume("volume1",
                           ns);
    for(int i = 0; i < 1024; i++)
    {
        writeToVolume(v1,
                      0,
                      v1->getClusterSize(),
                      "kristafke");
    }
    v1->scheduleBackendSync();
    destroyVolume(v1,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v1 = 0;

    ASSERT_NO_THROW(v1 = localRestart(ns));

    //    EXPECT_FALSE(v1->mdstore_was_rebuilt_);

    checkVolume(v1, 0, v1->getClusterSize(), "kristafke");
    checkCurrentBackendSize(v1);

}

TEST_P(LocalRestartTest, restartWithSnapshot)
{
    VolumeId vid1("volume1");
    // const backend::Namespace ns;

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    Volume* v1 = newVolume(vid1,
                           ns);
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v1, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        writeToVolume(v1,
                      0,
                      v1->getClusterSize(),
                      "kristafke");
        createSnapshot(v1, "Snapshot1");
    }

    ASSERT_FALSE(v1 = getVolume(vid1));
    ASSERT_NO_THROW(v1 = localRestart(ns));

    //     EXPECT_FALSE(v1->mdstore_was_rebuilt_);

    checkVolume(v1, 0, v1->getClusterSize(), "kristafke");
    checkCurrentBackendSize(v1);

}

TEST_P(LocalRestartTest, restartWithSnapshotButNoSCO)
{
    VolumeId vid1("volume1");
    //    const backend::Namespace ns1;

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();


    Volume* v1 = newVolume(vid1,
                           ns1);

    writeToVolume(v1,
                  0,
                  v1->getClusterSize(),
                  "kristafke");
    createSnapshot(v1, "snapshot1");
    waitForThisBackendWrite(v1);
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v1, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        createSnapshot(v1, "snapshot2");
    }

    v1 = 0;
    ASSERT_FALSE(v1 = getVolume(vid1));
    ASSERT_NO_THROW(v1 = localRestart(ns1));

    //     EXPECT_FALSE(v1->mdstore_was_rebuilt_);

    checkVolume(v1, 0, v1->getClusterSize(), "kristafke");

    waitForThisBackendWrite(v1);

    ASSERT_TRUE(v1->isSyncedToBackendUpTo(SnapshotName("snapshot2")));
    checkCurrentBackendSize(v1);
}

TEST_P(LocalRestartTest, RestartWithFOC)
{
    auto foc_ctx(start_one_foc());

    // const backend::Namespace nspace;

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

    writeToVolume(v,0,v->getClusterSize(), "bart");
    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v = 0;

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    sc->removeSCO(ns,
                  ClusterLocation(1).sco(),
                  true);
    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));

    //     EXPECT_FALSE(v->mdstore_was_rebuilt_);
}

namespace
{

enum class TLogCRCDisposition
{
    Regenerate,
    EffUp,
    Drop,
};

template<typename T>
class TLogFuckerUpper
    : public BasicDispatchProcessor
    , public T
{
public:
    typedef TLogFuckerUpper<T> this_type;

    TLogFuckerUpper(const fs::path& tlog_path,
                    const TLogId& tlog_id)
        : T()
        , tlog_path_(tlog_path / boost::lexical_cast<std::string>(tlog_id))
        , reader_(new TLogReader(tlog_path_))
    {}

    explicit TLogFuckerUpper(const fs::path& tlog_path)
        : T()
        , tlog_path_(tlog_path)
        , reader_(new TLogReader(tlog_path_))
    {}

    void
    processLoc(ClusterAddress a,
                const ClusterLocationAndHash& b)
    {
        ASSERT(writer_);

        auto r = T::process(a, b);
        if (r)
        {
            writer_->add(r->first, r->second);
        }
    }

    void
    processTLogCRC(CheckSum::value_type /* chksum */)
    {
        ASSERT(writer_);

        auto r = T::processTLogCRC();
        switch (r)
        {
        case TLogCRCDisposition::Regenerate:
            writer_->sync();
            break;
        case TLogCRCDisposition::EffUp:
            writer_->addWrongTLogCRC();
            break;
        case TLogCRCDisposition::Drop:
            break;
        }
    }

    void
    processSCOCRC(CheckSum::value_type cs)
    {
        ASSERT(writer_);

        auto r = T::processSCOCRC(cs);
        if (r)
        {
            writer_->add(CheckSum(*r));
        }
    }

    void
    processSync()
    {
        ASSERT(writer_);

        auto r = T::process();
        if (r)
        {
            writer_->add();
        }
    }

    void
    operator()()
    {
        // the TLogWriter's destructor needs to be called before doing anything
        // with the tlog, otherwise there might still be not-yet-written data in
        // the TLogWriter's buffer.
        const auto writer_path(FileUtils::create_temp_file(tlog_path_));
        writer_.reset(new TLogWriter(writer_path));

        reader_->for_each(*this);

        writer_.reset();
        FileUtils::safe_copy(writer_path, tlog_path_);
    }

    const fs::path tlog_path_;
    std::unique_ptr<TLogReaderInterface> reader_;
    std::unique_ptr<TLogWriter> writer_;
};

typedef boost::optional<std::pair<ClusterAddress,
                                  ClusterLocationAndHash> > maybe_ca_and_clh_type;
typedef boost::optional<CheckSum::value_type> maybe_cs_type;

struct FuckUpTLogCRC
{
    maybe_ca_and_clh_type
    process(ClusterAddress a,
            const ClusterLocationAndHash& b)
    {
        return maybe_ca_and_clh_type(std::make_pair(a, b));
    }

    TLogCRCDisposition
    processTLogCRC()
    {
        return TLogCRCDisposition::EffUp;
    }

    boost::optional<CheckSum::value_type>
    processSCOCRC(CheckSum::value_type cs)
    {
        return maybe_cs_type(cs);
    }

    bool
    process()
    {
        return true;
    }
};

struct DropTLogCRC
{
    maybe_ca_and_clh_type
    process(ClusterAddress a,
            const ClusterLocationAndHash& b)
    {
        return maybe_ca_and_clh_type(std::make_pair(a, b));
    }

    TLogCRCDisposition
    processTLogCRC()
    {
        return TLogCRCDisposition::Drop;
    }

    boost::optional<CheckSum::value_type>
    processSCOCRC(CheckSum::value_type cs)
    {
        return maybe_cs_type(cs);
    }

    bool
    process()
    {
        return true;
    }
};

}

TEST_P(LocalRestartTest, funnyTLogCRC)
{
    VolumeId vid1("volume1");
    // const backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    Volume* v1 = newVolume(vid1,
                           ns);
    ASSERT_TRUE(v1);

    writeToVolume(v1,
                  0,
                  v1->getClusterSize(),
                  "bdv");
    waitForThisBackendWrite(v1);
    const fs::path tlog_dir = VolManager::get()->getTLogPath(v1);
    OrderedTLogIds tlogs;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v1, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        writeToVolume(v1, 0, v1->getClusterSize(),"ar");
        getTLogsNotInBackend(vid1,
                         tlogs);
    }

    v1 = 0;
    ASSERT_FALSE(v1 = getVolume(vid1));

    for (const auto& tlog_id : tlogs)
    {
        TLogFuckerUpper<FuckUpTLogCRC> tlfck(tlog_dir,
                                             tlog_id);
        tlfck();
    }

    ASSERT_THROW(v1 = localRestart(ns),
                 TLogWrongCRC);
}

namespace
{

struct RemoveSync2TC
{
    maybe_ca_and_clh_type
    process(ClusterAddress a,
            const ClusterLocationAndHash& b)
    {
        return maybe_ca_and_clh_type(std::make_pair(a, b));
    }

    TLogCRCDisposition
    processTLogCRC()
    {
        return TLogCRCDisposition::Regenerate;
    }

    maybe_cs_type
    processSCOCRC(CheckSum::value_type cs)
    {
        return maybe_cs_type(cs);
    }

    bool
    process()
    {
        return false;
    }
};

}

// SINCE SYNCTOTC is gone thsi doesn't make any sense anymore
// Y42 write some tests for the clusters.
TEST_P(LocalRestartTest, DISABLED_NoSyncToTC)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    // const backend::Namespace ns1;

    Volume* v1 = newVolume(vid1,
                           ns);
    ASSERT_TRUE(v1);

    writeToVolume(v1,
                  0,
                  v1->getClusterSize(),
                  "bdv");
    waitForThisBackendWrite(v1);
    const fs::path tlog_dir = VolManager::get()->getTLogPath(v1);
    OrderedTLogIds tlogs;

    MetaDataStoreStats mds1;
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v1, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        writeToVolume(v1, 0, v1->getClusterSize(), "ar");
        getTLogsNotInBackend(vid1,
                         tlogs);

        v1->getMetaDataStore()->getStats(mds1);
        EXPECT_LT(0U, mds1.used_clusters);
    }

    v1 = 0;
    ASSERT_FALSE(v1 = getVolume(vid1));

    for (const auto& tlog_id : tlogs)
    {
        TLogFuckerUpper<RemoveSync2TC> tlfck(tlog_dir,
                                             tlog_id);
        tlfck();
    }

    ASSERT_NO_THROW(v1 = localRestart(ns));

    ASSERT_TRUE(v1);

    //    EXPECT_TRUE(v1->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v1->getMetaDataStore()->getStats(mds2);

    EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v1, 0, v1->getClusterSize(), "ar");
    checkCurrentBackendSize(v1);
}

TEST_P(LocalRestartTest, MetaDataStoreRunsAhead)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    // const backend::Namespace ns1;
    Volume* v1 = newVolume(vid1,
                           ns);
    ASSERT_TRUE(v1);

    writeToVolume(v1,
                  0,
                  v1->getClusterSize(),
                  "bdv");

    ASSERT_NO_THROW(createSnapshot(v1,"asnapshot"));

    writeToVolume(v1,
                  0,
                  v1->getClusterSize(),
                  "il");
    waitForThisBackendWrite(v1);

    MetaDataStoreStats mds1;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v1,
                                              2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        writeToVolume(v1, 0, v1->getClusterSize(),"ar");

        v1->getMetaDataStore()->getStats(mds1);
        EXPECT_LT(0U, mds1.used_clusters);

        v1->getMetaDataStore()->writeCluster(3,
                                             ClusterLocationAndHash(ClusterLocation(30),
                                                                    growWeed()));
    }

    v1 = 0;
    ASSERT_FALSE(v1 = getVolume(vid1));

    ASSERT_NO_THROW(v1 = localRestart(ns));

    ASSERT_TRUE(v1);
    // EXPECT_TRUE(v1->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v1->getMetaDataStore()->getStats(mds2);
    EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);
    ASSERT_TRUE(v1->isSyncedToBackendUpTo(SnapshotName("asnapshot")));
    checkVolume(v1, 0, v1->getClusterSize(), "ar");
    checkCurrentBackendSize(v1);
}

TEST_P(LocalRestartTest, RestartWithFOCAndRemovedSCO)
{
    auto foc_ctx(start_one_foc());
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

    const uint64_t cluster_size = v->getClusterSize();
    MetaDataStoreStats mds1;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i < (1 << 12); ++i)
        {
            writeToVolume(v,
                          i * (cluster_size / 512),
                          cluster_size,
                          "bart");
        }

        v->getMetaDataStore()->getStats(mds1);
    }
    v = 0;

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(1).sco());
    ASSERT_TRUE(sco_ptr.get());

    fs::remove(sco_ptr->path());
    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));

    ASSERT_TRUE(v);
    //    EXPECT_FALSE(v->mdstore_was_rebuilt_);
    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);

    EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    for(int i = 0; i < 1 << 12; ++i)
    {
        checkVolume(v,i*(cluster_size/512), cluster_size, "bart");
    }
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, RestartWithoutFOCAndRemovedSCO)
{
    //   const backend::Namespace ns;

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    Volume* v = newVolume("vol1",
                          ns);

    const uint64_t cluster_size = v->getClusterSize();

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i < 32; ++i)
        {
            writeToVolume(v,i* (cluster_size/512), cluster_size, "bart");

        }
    }

    v = 0;

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(1).sco());
    ASSERT_TRUE(sco_ptr.get());

    fs::remove(sco_ptr->path());
    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);

    //    EXPECT_TRUE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds;
    v->getMetaDataStore()->getStats(mds);
    // EXPECT_EQ(0, mds.used_clusters);
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, RestartWithFuckedUpFOCAndTruncatedSCO)
{
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);

    const uint64_t cluster_size = v->getClusterSize();
    MetaDataStoreStats mds1;

    {
        auto foc_ctx(start_one_foc());

        ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

        {
            SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                                  DeleteLocalData::F,
                                                  RemoveVolumeCompletely::F);
            for(int i = 0; i <  32; ++i)
            {
                writeToVolume(v, 0, cluster_size, "bart");
            }
            for(int i = 0; i < 32; ++i)
            {
                writeToVolume(v, 0, cluster_size, "immanuel");
            }
            v->getMetaDataStore()->getStats(mds1);
            // also disabled on default
            // EXPECT_LT(0, mds1.usead_clusters);
        }

        v = 0;
    }

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(2).sco());

    ASSERT_TRUE(sco_ptr.get());
    FileUtils::truncate(sco_ptr->path(),
                        16 * cluster_size);

    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);

    //    EXPECT_TRUE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);

    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v,0, cluster_size, "bart");
    checkCurrentBackendSize(v);
}

namespace
{

struct RemoveAllButFirstSync2TC
{
    RemoveAllButFirstSync2TC()
        : seen(false)
    {}

    maybe_ca_and_clh_type
    process(ClusterAddress a,
            const ClusterLocationAndHash& b)
    {
        return maybe_ca_and_clh_type(std::make_pair(a, b));
    }

    TLogCRCDisposition
    processTLogCRC()
    {
        return TLogCRCDisposition::Regenerate;
    }

    maybe_cs_type
    processSCOCRC(CheckSum::value_type cs)
    {
        return maybe_cs_type(cs);
    }

    bool
    process()
    {
        if (not seen)
        {
            seen = true;
            return true;
        }
        else
        {
            return false;
        }
    }


    bool seen;
};

}

TEST_P(LocalRestartTest, RestartWithMetaDataReplay)
{
    switch (metadata_backend_type())
    {
    case MetaDataBackendType::Arakoon:
    case MetaDataBackendType::MDS:
    case MetaDataBackendType::RocksDB:
        TODO("AR: fix at least for RocksDB?");
        {
            LOG_FATAL("This test cannot be run with an " <<
                      metadata_backend_type() << " metadatastore");
            SUCCEED(); // FAIL instead?
            break;
        }
    case MetaDataBackendType::TCBT:
        {
            //        const backend::Namespace ns;
            auto ns_ptr = make_random_namespace();

            const backend::Namespace& ns = ns_ptr->ns();

            VolumeId vid("vol1");

            Volume* v = newVolume(vid,
                                  ns);
            const uint64_t cluster_size = v->getClusterSize();
            fs::path mdstore = VolManager::get()->getMetaDataDBFilePath(v->get_config());

            fs::path tmp_mdstore = FileUtils::create_temp_file(mdstore);
            const fs::path tlog_dir = VolManager::get()->getTLogPath(v);
            OrderedTLogIds tlogs;

            MetaDataStoreStats mds1;

            {
                SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                                      2,
                                                      DeleteLocalData::F,
                                                      RemoveVolumeCompletely::F);
                for(int i = 0; i <  8; ++i)
                {
                    writeToVolume(v,0, cluster_size, "bart");
                }
                for(int i = 0; i <  8; ++i)
                {
                    writeToVolume(v,8, cluster_size, "bart");
                }

                v->sync();
                FileUtils::safe_copy(mdstore, tmp_mdstore);

                for(int i = 0; i < 16; ++i)
                {
                    writeToVolume(v,8, cluster_size, "immanuel");
                }

                for(int i = 0; i < 16; ++i)
                {
                    writeToVolume(v,0, cluster_size, "immanuel");
                }
                v->getMetaDataStore()->getStats(mds1);
                // EXPECT_LT(0, mds1.used_clusters);

                v->sync();
                getTLogsNotInBackend(vid,
                                 tlogs);
            }
            v = 0;
            ASSERT(tlogs.size() == 1);

            TLogFuckerUpper<RemoveAllButFirstSync2TC> tflk(tlog_dir,
                                                           tlogs[0]);
            tflk();

            fs::rename(tmp_mdstore, mdstore);

            ASSERT_NO_THROW(v = localRestart(ns));
            ASSERT_TRUE(v);
            //         EXPECT_TRUE(v->mdstore_was_rebuilt_);

            checkVolume(v,0, cluster_size, "immanuel");
            checkVolume(v,8, cluster_size, "immanuel");

            MetaDataStoreStats mds2;
            v->getMetaDataStore()->getStats(mds2);
            // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

            checkCurrentBackendSize(v);
        }
        break;
    }
}

TEST_P(LocalRestartTest, RestartWithHalfASCO)
{
    fun_with_half_a_sco(FOCMode::None);
}

TEST_P(LocalRestartTest, RestartWithFOCAndHalfASCO)
{
    fun_with_half_a_sco(FOCMode::Healthy);
}

TEST_P(LocalRestartTest, RestartWithFuckedUpFOCAndHalfASCO)
{
    fun_with_half_a_sco(FOCMode::Broken);
}

TEST_P(LocalRestartTest, WithFOCAndTruncatedSCO)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    //    const backend::Namespace ns;

    Volume* v = newVolume("vol1",
                          ns);
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

    const uint64_t cluster_size = v->getClusterSize();

    MetaDataStoreStats mds1;
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i <  32; ++i)
        {
            writeToVolume(v,0, cluster_size, "bart");
        }
        for(int i = 0; i < 32; ++i)
        {
            writeToVolume(v,0, cluster_size, "immanuel");
        }

        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }
    v = 0;

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(2).sco());

    ASSERT_TRUE(sco_ptr.get());
    FileUtils::truncate(sco_ptr->path(),
                        16 * cluster_size);

    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);
    //    EXPECT_FALSE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v,0, cluster_size, "immanuel");
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, RestartWithoutFOCAndTruncatedSCO)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    // const backend::Namespace ns;

    Volume* v = newVolume("vol1",
                          ns);
    const uint64_t cluster_size = v->getClusterSize();

    MetaDataStoreStats mds1;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i <  32; ++i)
        {
            writeToVolume(v,0, cluster_size, "bart");
        }
        for(int i = 0; i < 32; ++i)
        {
            writeToVolume(v,0, cluster_size, "immanuel");
        }
        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }
    v = 0;

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(2).sco());

    ASSERT_TRUE(sco_ptr.get());
    FileUtils::truncate(sco_ptr->path(),
                        16 * cluster_size);

    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);
    //    EXPECT_TRUE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v,0, cluster_size, "bart");
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, RestartWithFOCAndRemovedSCO2)
{
    auto foc_ctx(start_one_foc());

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    // const backend::Namespace ns;

    Volume* v = newVolume("vol1",
                          ns);
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

    const uint64_t cluster_size = v->getClusterSize();

    MetaDataStoreStats mds1;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i <  32; ++i)
        {
            writeToVolume(v,0, cluster_size, "bart");
        }
        for(int i = 0; i < 32; ++i)
        {
            writeToVolume(v,0, cluster_size, "immanuel");
        }
        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }

    v = 0;

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(2).sco());

    ASSERT_TRUE(sco_ptr.get());
    fs::remove(sco_ptr->path());

    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);
    //    EXPECT_FALSE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v,0, cluster_size, "immanuel");
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, RestartWithFOCAndFuckedUpSCO)
{
    auto foc_ctx(start_one_foc());

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    // const backend::Namespace ns;

    Volume* v = newVolume("vol1",
                          ns);
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

    const uint64_t cluster_size = v->getClusterSize();

    MetaDataStoreStats mds1;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i <  32; ++i)
        {
            writeToVolume(v,0, cluster_size, "bart");
        }
        for(int i = 0; i < 32; ++i)
        {
            writeToVolume(v,0, cluster_size, "immanuel");
        }

        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }
    v = 0;


    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(2).sco());

    ASSERT_TRUE(sco_ptr.get());
    // CachedSCOPtr sco_ptr2 = sc->findSCO(ns,
    //                                    ClusterLocation(3).sco());
    //     ASSERT_FALSE(sco_ptr2);
    FuckUpSCO(sco_ptr->path());
    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);
    //     EXPECT_FALSE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v,0, cluster_size, "immanuel");
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, RestartWithoutFOCAndFuckedUpSCO)
{

    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    Volume* v = newVolume("vol1",
                          ns);
    const uint64_t cluster_size = v->getClusterSize();

    MetaDataStoreStats mds1;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i <  32; ++i)
        {
            writeToVolume(v,0, cluster_size, "bart");
        }
        for(int i = 0; i < 32; ++i)
        {
            writeToVolume(v,0, cluster_size, "immanuel");
        }
        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }

    v = 0;

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(2).sco());

    ASSERT_TRUE(sco_ptr.get());
    // CachedSCOPtr sco_ptr2 = sc->findSCO(ns,
    //                                    ClusterLocation(3).sco());
    //     ASSERT_FALSE(sco_ptr2);
    FuckUpSCO(sco_ptr->path());
    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);
    //    EXPECT_TRUE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v,0, cluster_size, "bart");
    checkCurrentBackendSize(v);
}

namespace
{

struct FailOverRestartPutsSCOCRCInTLogProcessor
    : public CheckSCOCRCProcessor
{};

}

TEST_P(LocalRestartTest, CreateSnapshotPutsSCOCRCInTLog)
{
    //const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    VolumeId vid("vol1");

    Volume* v = newVolume(vid,
                          ns);

    VolumeConfig cfg = v->get_config();

    const uint64_t cluster_size = v->getClusterSize();
    writeToVolume(v,0, cluster_size, "immanuel");
    v->createSnapshot(SnapshotName("snap1"));
    writeToVolume(v,0, cluster_size, "bart");
    const fs::path tlogs_path = VolManager::get()->getTLogPath(cfg);
    OrderedTLogIds tlogs(v->getSnapshotManagement().getAllTLogs());

    ASSERT_TRUE(tlogs.size() > 0);
    std::shared_ptr<TLogReaderInterface>
        tlog_reader(makeCombinedTLogReader(tlogs_path,
                                           tlogs,
                                           v->getBackendInterface()->clone()));
    FailOverRestartPutsSCOCRCInTLogProcessor callback;
    ASSERT_NO_THROW(tlog_reader->for_each(callback));
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, BigWritesPutSCOCRCInTLog)
{
    //    const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    VolumeId vid("vol1");

    Volume* v = newVolume(vid,
                          ns);

    const uint64_t cluster_size = v->getClusterSize();
    writeToVolume(v,0, cluster_size, "immanuel");

    const uint64_t sco_size = v->getSCOMultiplier() * cluster_size;

    writeToVolume(v,0, sco_size, "bart");
    writeToVolume(v,0, sco_size, "arne");
    writeToVolume(v,0, sco_size, "wouter");
    VolumeConfig cfg = v->get_config();
    const fs::path tlogs_path = VolManager::get()->getTLogPath(cfg);
    const OrderedTLogIds tlogs(v->getSnapshotManagement().getAllTLogs());

    std::shared_ptr<TLogReaderInterface>
        tlog_reader(makeCombinedTLogReader(tlogs_path,
                                           tlogs,
                                           v->getBackendInterface()->clone()));

    FailOverRestartPutsSCOCRCInTLogProcessor callback;
    ASSERT_NO_THROW(tlog_reader->for_each(callback));
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, SetFailOVerPutsSCOCRCInTLog)
{
    auto foc_ctx(start_one_foc());

    //    const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    VolumeId vid("vol1");

    Volume* v = newVolume(vid,
                          ns);

    VolumeConfig cfg = v->get_config();

    const uint64_t cluster_size = v->getClusterSize();
    writeToVolume(v,0, cluster_size, "immanuel");
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

    writeToVolume(v,0, cluster_size, "bart");
    v->createSnapshot(SnapshotName("snap1"));
    writeToVolume(v,0, cluster_size, "arne");

    const fs::path tlogs_path = VolManager::get()->getTLogPath(cfg);
    const OrderedTLogIds tlogs(v->getSnapshotManagement().getAllTLogs());
    ASSERT_TRUE(tlogs.size() > 0);
    FailOverRestartPutsSCOCRCInTLogProcessor callback;
    std::shared_ptr<TLogReaderInterface>
        tlog_reader(makeCombinedTLogReader(tlogs_path,
                                           tlogs,
                                           v->getBackendInterface()->clone()));

    ASSERT_NO_THROW(tlog_reader->for_each(callback));
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, FailOverRestartPutsSCOCRCInTLog)
{
    auto foc_ctx(start_one_foc());

    //    const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    VolumeId vid("vol1");

    Volume* v = newVolume(vid,
                          ns);

    VolumeConfig cfg = v->get_config();
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

    const uint64_t cluster_size = v->getClusterSize();
    uint64_t clusters   = VolManager::get()->number_of_scos_in_tlog.value() *  v->getSCOMultiplier();

    for(unsigned i = 0; i <  clusters; ++i)
    {
        writeToVolume(v,0, cluster_size, "immanuel");
    }
    waitForThisBackendWrite(v);
    OrderedTLogIds tlogs;
    ASSERT_TRUE(tlogs.empty());

    v->getSnapshotManagement().getTLogsWrittenToBackend(tlogs);
    ASSERT_FALSE(tlogs.empty());

    MetaDataStoreStats mds1;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              2,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);

        uint64_t rand  = drand48() * v->getSCOMultiplier();
        for(unsigned i = 0; i <  clusters; ++i)
        {
            writeToVolume(v,0, cluster_size, "bart");
            if(clusters % v->getSCOMultiplier() == rand)
            {
                rand = drand48() * v->getSCOMultiplier();
                v->getDataStore()->finalizeCurrentSCO();
            }
        }
        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }
    v = 0;
    ASSERT_NO_THROW(restartVolume(cfg,
                                  PrefetchVolumeData::T));

    ASSERT_NO_THROW(v = findVolume(vid));
    ASSERT_TRUE(v);
    //    EXPECT_TRUE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v,0, cluster_size,"bart");

    const OrderedTLogIds tlogs2(v->getSnapshotManagement().getAllTLogs());

    ASSERT_TRUE(tlogs2.size() > tlogs.size());

    for(unsigned i = 0; i < tlogs.size(); i++)
    {
        ASSERT_EQ(tlogs[i],
                  tlogs2[i]);
    }
    FailOverRestartPutsSCOCRCInTLogProcessor callback;
    const fs::path tlogs_path = VolManager::get()->getTLogPath(cfg);
    for(unsigned i = tlogs.size() ; i < tlogs2.size(); ++i)
    {
        TLogReader tlog_reader(tlogs_path,
                               boost::lexical_cast<std::string>(tlogs2[i]),
                               v->getBackendInterface()->clone());

        ASSERT_NO_THROW(tlog_reader.for_each(callback));
    }
    checkCurrentBackendSize(v);
}

//Disabled: see VOLDRV-1042
TEST_P(LocalRestartTest, DISABLED_NoLocalTLogs)
{
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);
    const fs::path tlog_path(VolManager::get()->getTLogPath(v));
    const uint64_t clusters_in_tlog =
        VolManager::get()->number_of_scos_in_tlog.value() *  v->getSCOMultiplier();
    const uint64_t clusters_to_write = clusters_in_tlog - 1;

    const TLogId init_tlog(v->getSnapshotManagement().getCurrentTLogId());

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);

        writeClusters(v, clusters_to_write);
        ASSERT_EQ(init_tlog,
                  v->getSnapshotManagement().getCurrentTLogId());
    }

    v = 0;

    fs::remove(tlog_path / boost::lexical_cast<std::string>(init_tlog));
    ASSERT_NO_THROW(v = localRestart(ns));
}

TEST_P(LocalRestartTest, NonNeededTLogsMissing)
{
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);
    const fs::path tlog_path = VolManager::get()->getTLogPath(v);
    uint64_t clusters_in_tlog   = VolManager::get()->number_of_scos_in_tlog.value() *  v->getSCOMultiplier();
    uint64_t clusters_to_write = (clusters_in_tlog / 2) + 2;

    const TLogId init_tlog(v->getSnapshotManagement().getCurrentTLogId());
    TLogId next_tlog;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);

        writeClusters(v, clusters_to_write);
        v->sync();
        writeClusters(v, clusters_to_write);
        next_tlog = v->getSnapshotManagement().getCurrentTLogId();
        ASSERT_NE(init_tlog,
                  next_tlog);

    }
    v = 0;
    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(2).sco());

    ASSERT_TRUE(sco_ptr.get());
    CachedSCOPtr sco_ptr2 = sc->findSCO(ns,
                                        ClusterLocation(3).sco());
    ASSERT_TRUE(sco_ptr2.get());
    FuckUpSCO(sco_ptr->path());
    sc->disableNamespace(ns);

    fs::remove(tlog_path / boost::lexical_cast<std::string>(next_tlog));
    ASSERT_NO_THROW(v = localRestart(ns));
}

TEST_P(LocalRestartTest,  TLogRolloverRestartTest)
{
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);

    uint64_t clusters_in_tlog   = VolManager::get()->number_of_scos_in_tlog.value() *  v->getSCOMultiplier();
    uint64_t clusters_to_write  = clusters_in_tlog + 3;

    MetaDataStoreStats mds1;
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        const TLogId init_tlog(v->getSnapshotManagement().getCurrentTLogId());

        writeClusters(v, clusters_to_write);

        ASSERT_NE(init_tlog,
                  v->getSnapshotManagement().getCurrentTLogId());
        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }
    v = 0;

    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);
    //    EXPECT_FALSE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkClusters(v, clusters_to_write);
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest,  NoTlogCRCFound)
{
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);

    const fs::path tlog_dir = VolManager::get()->getTLogPath(v);

    uint64_t clusters_in_tlog   = VolManager::get()->number_of_scos_in_tlog.value() *  v->getSCOMultiplier();
    uint64_t clusters_to_write  = clusters_in_tlog + 3;

    boost::optional<TLogId> init_tlog;

    MetaDataStoreStats mds1;
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);

        init_tlog = v->getSnapshotManagement().getCurrentTLogId();

        writeClusters(v, clusters_to_write);

        ASSERT_NE(init_tlog,
                  v->getSnapshotManagement().getCurrentTLogId());
        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }

    v = 0;

    TLogFuckerUpper<DropTLogCRC> fupper(tlog_dir,
                                        (*init_tlog));
    fupper();

    ASSERT_THROW(v = localRestart(ns), TLogWithoutFinalCRC);
}

namespace
{

struct RemoveSCOCRC
{
    maybe_ca_and_clh_type
    process(ClusterAddress a,
            const ClusterLocationAndHash& b)
    {
        return maybe_ca_and_clh_type(std::make_pair(a, b));
    }

    TLogCRCDisposition
    processTLogCRC()
    {
        return TLogCRCDisposition::Regenerate;
    }

    maybe_cs_type
    processSCOCRC(CheckSum::value_type /* cs */)
    {
        return maybe_cs_type();
    }

    bool process()
    {
        return true;
    }
};

}

TEST_P(LocalRestartTest, RestartWithMissingSCOCRC)
{
    VolumeId vid1("volume1");
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = newVolume(vid1,
                           ns);
    ASSERT_TRUE(v1);



    writeToVolume(v1,
                  0,
                  v1->getClusterSize(),
                  "bdv");

    waitForThisBackendWrite(v1);
    const fs::path tlog_dir = VolManager::get()->getTLogPath(v1);
    OrderedTLogIds tlogs;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v1,
                                              2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i < 63; ++i)
        {
            writeToVolume(v1, 0, v1->getClusterSize(),"ar");
        }

        getTLogsNotInBackend(vid1,
                         tlogs);
    }
    v1 = 0;

    ASSERT_FALSE(v1 = getVolume(vid1));
    for (const auto& tlog_id : tlogs)
    {
        TLogFuckerUpper<RemoveSCOCRC> tlfck(tlog_dir,
                                            tlog_id);
        tlfck();
    }
    ASSERT_THROW(v1 = localRestart(ns), SCOSwitchWithoutSCOCRC);
    ASSERT_FALSE(v1);
}

TEST_P(LocalRestartTest, RestartWithFOCAndFuckedUpSCO2)
{
    auto foc_ctx(start_one_foc());

    //     const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    Volume* v = newVolume("vol1",
                          ns);
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

    const uint64_t cluster_size = v->getClusterSize();
    MetaDataStoreStats mds1;
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i <  32; ++i)
        {
            writeToVolume(v,0, cluster_size, "bart");
        }
        for(int i = 0; i < 32; ++i)
        {
            writeToVolume(v,0, cluster_size, "immanuel");
        }
        for(int i = 0; i < 32; ++i)
        {
            writeToVolume(v,0, cluster_size, "immanuel");
        }

        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }
    v = 0;

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(2).sco());

    ASSERT_TRUE(sco_ptr.get());
    CachedSCOPtr sco_ptr2 = sc->findSCO(ns,
                                        ClusterLocation(3).sco());
    ASSERT_TRUE(sco_ptr2.get());
    FuckUpSCO(sco_ptr->path());
    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);
    //    EXPECT_FALSE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v,0, cluster_size, "immanuel");
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, RestartWithoutFOCAndFuckedUpSCO2)
{
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);
    const uint64_t cluster_size = v->getClusterSize();

    MetaDataStoreStats mds1;
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i <  32; ++i)
        {
            writeToVolume(v,0, cluster_size, "bart");
        }
        for(int i = 0; i < 32; ++i)
        {
            writeToVolume(v,0, cluster_size, "immanuel");
        }
        for(int i = 0; i < 32; ++i)
        {
            writeToVolume(v,0, cluster_size, "immanuel");
        }
        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }

    v = 0;

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOAccessData sad(ns);

    sc->enableNamespace(ns,
                        0,
                        100,
                        sad);
    CachedSCOPtr sco_ptr = sc->findSCO(ns,
                                       ClusterLocation(2).sco());

    ASSERT_TRUE(sco_ptr.get());
    CachedSCOPtr sco_ptr2 = sc->findSCO(ns,
                                        ClusterLocation(3).sco());
    ASSERT_TRUE(sco_ptr2.get());
    FuckUpSCO(sco_ptr->path());
    sc->disableNamespace(ns);

    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);
    //    EXPECT_TRUE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);
}

TEST_P(LocalRestartTest, RestartWithFOCAndOfflinedMountPoint)
{
    auto foc_ctx(start_one_foc());

    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    Volume* v = newVolume("vol1",
                          ns);
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

    const uint64_t cluster_size = v->getClusterSize();

    MetaDataStoreStats mds1;
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        for(int i = 0; i < (1 << 12); ++i)
        {
            writeToVolume(v,i* (cluster_size/512), cluster_size, "bart");
        }
        v->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }
    v = 0;

    SCOCache* sc = VolManager::get()->getSCOCache();
    SCOCacheMountPointsInfo mp_info;

    sc->getMountPointsInfo(mp_info);

    ASSERT_TRUE( mp_info.size() > 1);
    sc->removeMountPoint(mp_info.begin()->first);


    ASSERT_NO_THROW(v = localRestart(ns));
    ASSERT_TRUE(v);
    //    EXPECT_FALSE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    for(int i = 0; i < 1 << 12; ++i)
    {
        checkVolume(v,i*(cluster_size/512), cluster_size, "bart");
    }
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, NothingToRestartLocallyFrom)
{
    VolumeId vid1("volume1");
    //const backend::Namespace ns1;

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume(vid1,
                           ns1);
    writeToVolume(v1,
                  0,
                  v1->getClusterSize(),
                  "bdv");
    waitForThisBackendWrite(v1);
    v1->sync();

    MetaDataStoreStats mds1;
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v1,
                                              2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        writeToVolume(v1, 0, v1->getClusterSize(),"ar");
        v1->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }
    v1 = 0;

    ASSERT_FALSE(v1 = getVolume(vid1));
    ASSERT_NO_THROW(v1 = localRestart(ns1));
    ASSERT_TRUE(v1);
    //    EXPECT_FALSE(v1->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v1->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v1, 0, v1->getClusterSize(), "ar");
    checkCurrentBackendSize(v1);
}

TEST_P(LocalRestartTest, MDStoreOutToLunch)
{
    switch (metadata_backend_type())
    {
    case MetaDataBackendType::Arakoon:
    case MetaDataBackendType::MDS:
        {
            LOG_FATAL("This test cannot be run with an " <<
                      metadata_backend_type() << " metadatastore");
            SUCCEED(); // FAIL instead?
            break;
        }
    case MetaDataBackendType::RocksDB:
    case MetaDataBackendType::TCBT:
        {
            VolumeId vid1("volume1");
            // const backend::Namespace ns1;
            auto ns_ptr = make_random_namespace();

            const backend::Namespace& ns1 = ns_ptr->ns();

            Volume* v1 = newVolume(vid1,
                                   ns1);
            writeToVolume(v1,
                          0,
                          v1->getClusterSize(),
                          "bdv");
            waitForThisBackendWrite(v1);
            v1->sync();

            const fs::path mdstore_path = VolManager::get()->getMetaDataDBFilePath(v1->get_config());

            MetaDataStoreStats mds1;
            {
                SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v1,
                                                      2,
                                                      DeleteLocalData::F,
                                                      RemoveVolumeCompletely::F);
                writeToVolume(v1, 0, v1->getClusterSize(),"ar");
                v1->getMetaDataStore()->getStats(mds1);
                // EXPECT_LT(0, mds1.used_clusters);
            }
            EXPECT_TRUE(fs::exists(mdstore_path));

            EXPECT_NO_THROW(fs::remove(mdstore_path));
            EXPECT_FALSE(fs::exists(mdstore_path));

            v1 = 0;

            ASSERT_FALSE(v1 = getVolume(vid1));
            ASSERT_NO_THROW(v1 = localRestart(ns1));
            ASSERT_TRUE(v1);
            //    EXPECT_TRUE(v1->mdstore_was_rebuilt_);

            MetaDataStoreStats mds2;
            v1->getMetaDataStore()->getStats(mds2);
            // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

            checkVolume(v1, 0, v1->getClusterSize(), "ar");
            checkCurrentBackendSize(v1);
        }
        break;
    }
}

TEST_P(LocalRestartTest, MDStoreOutToLunchWithSnapshotsFile)
{
    VolumeId vid1("volume1");
    //const backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume(vid1,
                           ns1);
    writeToVolume(v1,
                  0,
                  v1->getClusterSize(),
                  "bdv");
    waitForThisBackendWrite(v1);
    v1->sync();

    const fs::path mdstore_path = VolManager::get()->getMetaDataDBFilePath(v1->get_config());

    MetaDataStoreStats mds1;
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v1,
                                              2,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);
        writeToVolume(v1, 0, v1->getClusterSize(),"ar");
        v1->getMetaDataStore()->getStats(mds1);
        // EXPECT_LT(0, mds1.used_clusters);
    }
    EXPECT_FALSE(fs::exists(mdstore_path));

    v1 = 0;
    ASSERT_FALSE(v1 = getVolume(vid1));

    ASSERT_THROW(v1 = localRestart(ns1),
                 fungi::IOException);
}

TEST_P(LocalRestartTest, testRescheduledSCOS)
{
    VolumeId v1("volume1");
    // const backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v = newVolume(v1,
                          ns1);
    VolumeConfig cfg = v->get_config();
    writeToVolume(v, 0, 4096, "doh");
    ClusterLocation l(1);

    std::string scoptr_name =
        v->getDataStore()->getSCO(l.sco(), 0)->path().string();

    v->createSnapshot(SnapshotName("first"));

    waitForThisBackendWrite(v);

    MetaDataStoreStats mds1;
    v->getMetaDataStore()->getStats(mds1);
    // EXPECT_LT(0, mds1.used_clusters);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    // // make ze cleanslate ze inlined
    // VolManager* man = VolManager::get();
    // fs::remove_all(man->getTLogPath(cfg));
    // fs::remove_all(man->getMetaDataPath(cfg));
    // Fuck with the SCOCache in a mean and unbecoming way.
    struct stat st;
    int ret = ::stat(scoptr_name.c_str(), &st);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(st.st_mode bitand S_ISVTX);
    ret = ::chmod(scoptr_name.c_str(), st.st_mode ^ S_ISVTX);
    v = 0;
    // This will assert since it reschedules the sole sco to backend
    ASSERT_NO_THROW(v = localRestart(ns1));
    ASSERT_TRUE(v);
    //    EXPECT_FALSE(v->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    waitForThisBackendWrite(v);
    checkCurrentBackendSize(v);
}

TEST_P(LocalRestartTest, restartClone)
{
    // const backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume("volume1",
                           ns1);
    for(int i = 0; i < 1024; i++)
    {
        writeToVolume(v1,
                      0,
                      v1->getClusterSize(),
                      "kristafke");
    }
    createSnapshot(v1,"snap1");
    waitForThisBackendWrite(v1);
    // const backend::Namespace ns2;
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    Volume* v2 = createClone("volume2",
                             ns2,
                             ns1,
                             "snap1");
    ASSERT_TRUE(v2);

    for(int i = 0; i < 1; i++)
    {
        writeToVolume(v2,
                      v2->getClusterSize(),
                      v2->getClusterSize(),
                      "kristafke");
    }

    MetaDataStoreStats mds1;
    v2->getMetaDataStore()->getStats(mds1);
    // EXPECT_LT(0, mds1.used_clusters);

    destroyVolume(v1,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    destroyVolume(v2,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    v1 = 0;
    v2 = 0;

    ASSERT_NO_THROW(v2 = localRestart(ns2));
    ASSERT(v2);
    //    EXPECT_FALSE(v2->mdstore_was_rebuilt_);

    MetaDataStoreStats mds2;
    v2->getMetaDataStore()->getStats(mds2);
    // EXPECT_EQ(mds1.used_clusters, mds2.used_clusters);

    checkVolume(v2, v2->getClusterSize(), v2->getClusterSize(), "kristafke");
    checkCurrentBackendSize(v2);
}


namespace
{
class RemoveAfterClusterAddress
{
public:
    RemoveAfterClusterAddress()
        : seen_(false)
        , sco_crcs_seen_(0)
    {}

    void
    address(ClusterAddress ca)
    {
        ca_ = ca;
    }

    maybe_ca_and_clh_type
    process(ClusterAddress a,
            const ClusterLocationAndHash& b)
    {
        if (ca_ and *ca_ == a)
        {
            seen_ = true;
            return maybe_ca_and_clh_type(std::make_pair(a, b));
        }
        else
        {
            return seen_ ?
                maybe_ca_and_clh_type() :
                maybe_ca_and_clh_type(std::make_pair(a, b));
        }
    }

    TLogCRCDisposition
    processTLogCRC()
    {
        return seen_ ? TLogCRCDisposition::Drop : TLogCRCDisposition::Regenerate;
    }

    maybe_cs_type
    processSCOCRC(CheckSum::value_type cs)
    {
        if (not seen_)
        {
            ++sco_crcs_seen_;
            return maybe_cs_type(cs);
        }
        else
        {
            return maybe_cs_type();
        }
    }

    bool
    process()
    {
        return not seen_;
    }

    bool
    seen()
    {
        return seen_;
    }

    unsigned
    sco_crcs_seen()
    {
        return sco_crcs_seen_;
    }

private:
    boost::optional<ClusterAddress> ca_;
    bool seen_;
    unsigned sco_crcs_seen_;
};

}

TEST_P(LocalRestartTest, LostMDStoreCacheAndNoUsableLocalData)
{
    const std::string vname("volume");
    // const backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    auto v = newVolume(vname,
                       ns1);

    const auto max_tlog_entries = v->getSnapshotManagement().maxTLogEntries();
    const auto cluster_size = v->getClusterSize();
    const auto cluster_mult = v->getClusterMultiplier();

    const std::string pattern1("purportedly safe");
    const std::string pattern2("abandon all hope");

    const fs::path tlog_path(seq_write_and_drop_mdstore_cache_and_destroy(v,
                                                                          pattern1,
                                                                          pattern2));
    EXPECT_TRUE(fs::exists(tlog_path)) << tlog_path;

    TLogFuckerUpper<RemoveAfterClusterAddress> fupper(tlog_path);

    fupper.address(max_tlog_entries + 1);
    fupper();

    EXPECT_TRUE(fupper.seen());
    EXPECT_EQ(0U, fupper.sco_crcs_seen());

    ASSERT_NO_THROW(v = localRestart(ns1));
    ASSERT_TRUE(v);
    //     EXPECT_TRUE(v->mdstore_was_rebuilt_);

    checkVolume(v,
                0,
                max_tlog_entries * cluster_size,
                pattern1);

    checkVolume(v,
                max_tlog_entries * cluster_mult,
                max_tlog_entries * cluster_size,
                "\0");
    checkCurrentBackendSize(v);
}

// Cf. VOLDRV-867 and VOLDRV-870.
TEST_P(LocalRestartTest, LostMDStoreCacheAndUsableLocalData)
{
    const std::string vname("volume");
    // const backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    auto v = newVolume(vname,
                       ns1);

    const auto max_tlog_entries = v->getSnapshotManagement().maxTLogEntries();
    const auto sco_mult = v->getSCOMultiplier();
    const auto cluster_size = v->getClusterSize();
    const auto cluster_mult = v->getClusterMultiplier();

    const std::string pattern1("purportedly safe");
    const std::string pattern2("at least partially lost");

    const fs::path tlog_path(seq_write_and_drop_mdstore_cache_and_destroy(v,
                                                                          pattern1,
                                                                          pattern2));

    EXPECT_TRUE(fs::exists(tlog_path)) << tlog_path;

    TLogFuckerUpper<RemoveAfterClusterAddress> fupper(tlog_path);

    fupper.address(max_tlog_entries + sco_mult + 1);
    fupper();

    EXPECT_TRUE(fupper.seen());
    EXPECT_EQ(1U, fupper.sco_crcs_seen());

    ASSERT_NO_THROW(v = localRestart(ns1));
    ASSERT(v);
    //    EXPECT_TRUE(v->mdstore_was_rebuilt_);

    checkVolume(v,
                0,
                max_tlog_entries * cluster_size,
                pattern1);

    checkVolume(v,
                max_tlog_entries * cluster_mult,
                sco_mult * cluster_size,
                pattern2);

    checkVolume(v,
                (max_tlog_entries + sco_mult) * cluster_mult,
                sco_mult * cluster_size,
                "\0");
    checkCurrentBackendSize(v);
}

// Disabled as of VOLDRV-1015 - we now have the corking mechanism, so this can probably
// go away.
//Cf. VOLDRV-873
TEST_P(LocalRestartTest, DISABLED_ReliabilityOfTheBigOneIfTheMDStoreLRUWouldWorkAsExpected)
{
    // "The Big One" is assumed to be the last stable mdstore point (i.e. the last
    // SYNC TC in a tlog), which means that we assume any writes preceding the "Big One"
    // to be present (persisted) in the metadata store after an unclean shutdown.
    // However, the LRU cache on top of the metadata database might interfere ...
    //
    // Idea:
    // * warm the cache: mdstore_cache_pages writes to the first entry in each cache page
    // * write "A" to CA 0
    // * write "B" to CA "mdstore_page_entries" (first entry of second page) - TLog needs
    //   to roll over here
    // * make sure the tlog hits the backend, then block backend writes
    // * write X to CA 1
    // * write to the first entry in each cached page
    // * write to the first entry in an non-cached page -> the page containing write
    //   B is written out
    // * drop the mdstore cache, destroy the volume, truncate the last tlog within its
    //   first SCO
    // * restart the volume
    // * read from CA0 - the expectation is to get back "A"
    //
    // -> with the above, we need the tlog to contain 2 more entries than the
    //    mdstore has cache pages, and an mdstore page needs to have at least 2 entries.
    // So:

    const auto sco_mult = SCOMultiplier(4);
    const auto max_tlog_entries = scos_per_tlog * sco_mult;
    const auto page_entries = CachePage::capacity();

    ASSERT_LT(1U, page_entries);

    const auto cluster_mult = VolumeConfig::default_cluster_multiplier();
    const auto cluster_size = VolumeConfig::default_lba_size() * cluster_mult;

    ASSERT_LT(3U, max_tlog_entries);

    const auto mdstore_cache_pages = max_tlog_entries - 2;

    const std::string vname("volume");
    // const backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    auto v = newVolume(vname,
                       ns1,
                       VolumeSize(2 * page_entries * mdstore_cache_pages * cluster_size),
                       sco_mult,
                       default_lba_size(),
                       default_cluster_mult(),
                       mdstore_cache_pages);

    EXPECT_EQ(max_tlog_entries, v->getSnapshotManagement().maxTLogEntries());
    EXPECT_EQ(sco_mult, v->getSCOMultiplier());

    TLogId tlog1;

    {
        OrderedTLogIds tlogs;
        v->getSnapshotManagement().getTLogsNotWrittenToBackend(tlogs);
        EXPECT_EQ(1U, tlogs.size());
        tlog1 = tlogs.front();
    }

    const std::string pattern1("first page entry, warming up");
    for (uint32_t i = 0; i < mdstore_cache_pages; ++i)
    {
        writeToVolume(v,
                      i * cluster_mult * page_entries,
                      cluster_size,
                      pattern1);
    }

    const std::string pattern2("first page entry, before the big one");
    writeToVolume(v,
                  0,
                  cluster_size,
                  pattern2);

    const std::string pattern3("first page entry on second page aka The Big One");
    writeToVolume(v,
                  cluster_mult * page_entries,
                  cluster_size,
                  pattern3);

    // this should've triggered a tlog rollover
    waitForThisBackendWrite(v);

    {
        OrderedTLogIds tlogs;
        v->getSnapshotManagement().getTLogsWrittenToBackend(tlogs);
        EXPECT_EQ(1U, tlogs.size());
        EXPECT_EQ(tlog1, tlogs.front());
    }

    TLogId tlog2;

    {
        OrderedTLogIds tlogs;
        v->getSnapshotManagement().getTLogsNotWrittenToBackend(tlogs);
        EXPECT_EQ(1U, tlogs.size());
        tlog2 = tlogs.front();
    }

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              2,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);

        const std::string pattern4("second entry of the first page");

        writeToVolume(v,
                      cluster_mult,
                      cluster_size,
                      pattern4);

        const std::string pattern5("first page entry, after the big one");

        // write to the remaining pages *and* push out The Big One
        for (uint32_t i = 2; i <= mdstore_cache_pages; ++i)
        {
            writeToVolume(v,
                          i * cluster_mult * page_entries,
                          cluster_size,
                          pattern5);
        }

        // Typically not needed anymore as the corking gives control over what
        // we allow into the backend, so use that should you ever need it again.
        // dropMetaDataStoreCache(*v);
    }

    TLogFuckerUpper<RemoveAfterClusterAddress> fupper(VolManager::get()->getTLogPath(v),
                                                      tlog2);

    fupper.address(2 * page_entries);
    fupper();

    EXPECT_TRUE(fupper.seen());
    EXPECT_EQ(0U, fupper.sco_crcs_seen());

    ASSERT_NO_THROW(v = localRestart(ns1));
    ASSERT_TRUE(v);
    //    EXPECT_TRUE(v->mdstore_was_rebuilt_);

    // the big one
    checkVolume(v,
                cluster_mult * page_entries,
                cluster_size,
                pattern3);

    // the one before
    checkVolume(v,
                0,
                cluster_size,
                pattern2);
    checkCurrentBackendSize(v);
}

//Cf. VOLDRV-887
TEST_P(LocalRestartTest, SanityCheckOfTheBackendMissingTLog1)
{
    const uint32_t num_tlogs_on_backend = 3;
    Volume * the_parent = 0;
    Volume * the_clone = 0;
    std::unique_ptr<WithRandomNamespace> parent_ns;
    std::unique_ptr<WithRandomNamespace> clone_ns;

    prepare_sanity_check_tests(num_tlogs_on_backend,
                               the_parent,
                               the_clone,
                               parent_ns,
                               clone_ns);

    OrderedTLogIds tlogs_on_backend;
    the_clone->getSnapshotManagement().getTLogsWrittenToBackend(tlogs_on_backend);
    BackendInterfacePtr clone_bi = the_clone->getBackendInterface()->clone();
    backend::Namespace restartnmspace = the_clone->getNamespace();

    destroyVolume(the_clone,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    clone_bi->remove(boost::lexical_cast<std::string>(tlogs_on_backend.front()),
                     ObjectMayNotExist::T);

    // this used to throw but was only working
    localRestart(restartnmspace);
}

//Cf. VOLDRV-887
TEST_P(LocalRestartTest, SanityCheckOfTheBackendMissingTLog2)
{
    const uint32_t num_tlogs_on_backend = 3;
    Volume * the_parent = 0;
    Volume * the_clone = 0;
    std::unique_ptr<WithRandomNamespace> parent_ns;
    std::unique_ptr<WithRandomNamespace> clone_ns;

    prepare_sanity_check_tests(num_tlogs_on_backend,
                               the_parent,
                               the_clone,
                               parent_ns,
                               clone_ns);

    OrderedTLogIds tlogs_on_backend;
    the_clone->getSnapshotManagement().getTLogsWrittenToBackend(tlogs_on_backend);
    BackendInterfacePtr clone_bi = the_clone->getBackendInterface()->clone();
    backend::Namespace restartnmspace = the_clone->getNamespace();

    destroyVolume(the_clone,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    clone_bi->remove(boost::lexical_cast<std::string>(tlogs_on_backend.back()));

    ASSERT_THROW(localRestart(restartnmspace),
                 std::exception);
}

//Cf. VOLDRV-887
TEST_P(LocalRestartTest, SanityCheckOfTheBackendMissingParentTLog)
{
    const uint32_t num_tlogs_on_backend = 3;
    Volume* parent = 0;
    Volume* clone = 0;
    std::unique_ptr<WithRandomNamespace> parent_ns;
    std::unique_ptr<WithRandomNamespace> clone_ns;


    prepare_sanity_check_tests(num_tlogs_on_backend,
                               parent,
                               clone,
                               parent_ns,
                               clone_ns);

    const backend::Namespace nspace(clone->getNamespace());

    destroyVolume(clone,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    const OrderedTLogIds
        tlogs_on_parent_backend(parent->getSnapshotManagement().getTLogsWrittenToBackend());
    BackendInterfacePtr parent_bi(parent->getBackendInterface()->clone());
    parent_bi->remove(boost::lexical_cast<std::string>(tlogs_on_parent_backend[1]));

    ASSERT_NO_THROW(localRestart(nspace));
}

//Cf. VOLDRV-887
TEST_P(LocalRestartTest, SanityCheckOfTheBackendMissingUnusedParentTLog)
{
    const uint32_t num_tlogs_on_backend = 3;
    Volume* parent = 0;
    Volume* clone = 0;
    std::unique_ptr<WithRandomNamespace> parent_ns;
    std::unique_ptr<WithRandomNamespace> clone_ns;


    prepare_sanity_check_tests(num_tlogs_on_backend,
                               parent,
                               clone,
                               parent_ns,
                               clone_ns);

    const backend::Namespace nspace(clone->getNamespace());

    destroyVolume(clone,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    const OrderedTLogIds
        tlogs_on_parent_backend(parent->getSnapshotManagement().getTLogsWrittenToBackend());
    BackendInterfacePtr parent_bi(parent->getBackendInterface()->clone());

    parent_bi->remove(boost::lexical_cast<std::string>(tlogs_on_parent_backend[3])); //snapshot from which is cloned contains only tlogs 0,1,2

    ASSERT_NO_THROW(localRestart(nspace));
}

//Cf. VOLDRV-887
TEST_P(LocalRestartTest, SanityCheckOfTheBackendMissingSnapshotObject)
{
    const uint32_t num_tlogs_on_backend = 3;
    Volume* parent = 0;
    Volume* clone = 0;
    std::unique_ptr<WithRandomNamespace> parent_ns;
    std::unique_ptr<WithRandomNamespace> clone_ns;

    prepare_sanity_check_tests(num_tlogs_on_backend,
                               parent,
                               clone,
                               parent_ns,
                               clone_ns);

    BackendInterfacePtr clone_bi(clone->getBackendInterface()->clone());
    const backend::Namespace nspace(clone->getNamespace());

    destroyVolume(clone,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    clone_bi->remove(snapshotFilename());

    //own backend snapshots.xml should not be consulted on local restart
    ASSERT_NO_THROW(clone = localRestart(nspace));
    //and volume should recover from it

    scheduleBackendSync(clone);
    while(not clone->isSyncedToBackend())
    {
        sleep(1);
    }

    ASSERT_TRUE(clone_bi->objectExists(snapshotFilename()));

    VolumeConfig cfg(clone->get_config());

    destroyVolume(clone,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    ASSERT_NO_THROW(restartVolume(cfg));
}

//Cf. VOLDRV-887
TEST_P(LocalRestartTest, SanityCheckOfTheBackendMissingParentSnapshotObject)
{
    const uint32_t num_tlogs_on_backend = 3;
    Volume* parent = 0;
    Volume* clone = 0;
    std::unique_ptr<WithRandomNamespace> parent_ns;
    std::unique_ptr<WithRandomNamespace> clone_ns;

    prepare_sanity_check_tests(num_tlogs_on_backend,
                               parent,
                               clone,
                               parent_ns,
                               clone_ns);

    const backend::Namespace nspace(clone->getNamespace());

    destroyVolume(clone,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    BackendInterfacePtr parent_bi(parent->getBackendInterface()->clone());
    parent_bi->remove(snapshotFilename());

    ASSERT_THROW(clone = localRestart(nspace),
                 std::exception);
}

TEST_P(LocalRestartTest, fall_back_to_backend_restart)
{
    auto ns(make_random_namespace());
    Volume* v = newVolume(*ns);

    const fs::path meta(VolManager::get()->getMetaDataPath(v));
    const fs::path tlogs(VolManager::get()->getMetaDataPath(v));

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    ASSERT_FALSE(fs::exists(meta));
    ASSERT_FALSE(fs::exists(tlogs));

    ASSERT_THROW(localRestart(ns->ns(),
                              FallBackToBackendRestart::F),
                 std::exception);

    localRestart(ns->ns(),
                 FallBackToBackendRestart::T);
}

INSTANTIATE_TEST(LocalRestartTest);

}

// Local Variables: **
// mode: c++ **
// End: **
