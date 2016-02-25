// Copyright 2015 iNuron NV
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

#include "MDSTestSetup.h"
#include "VolManagerTestSetup.h"

#include <functional>
#include <future>

#include <youtils/Assert.h>
#include <youtils/ScopeExit.h>
#include <youtils/SourceOfUncertainty.h>
#include <youtils/System.h>
#include <youtils/wall_timer.h>

#include <backend/BackendTestSetup.h>

#include "../Api.h"
#include "../CachedMetaDataPage.h"
#include "../CachedMetaDataStore.h"
#include "../MDSMetaDataBackend.h"
#include "../MetaDataBackendConfig.h"
#include "../metadata-server/ClientNG.h"
#include "../metadata-server/Manager.h"
#include "../Scrubber.h"
#include "../ScrubberAdapter.h"
#include "../ScrubReply.h"
#include "../ScrubWork.h"
#include "../TLogReaderUtils.h"

namespace volumedrivertest
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace mds = metadata_server;
namespace yt = youtils;
namespace ytt = youtilstest;

using namespace volumedriver;
using namespace std::literals::string_literals;

class MDSVolumeTest
    : public VolManagerTestSetup
{
protected:
    DECLARE_LOGGER("MDSVolumeTest");

    MDSVolumeTest()
        : VolManagerTestSetup("MDSVolumeTest")
    {}

    virtual ~MDSVolumeTest() = default;

    virtual void
    SetUp()
    {
        VolManagerTestSetup::SetUp();
        mds_manager_ = mds_test_setup_->make_manager(cm_,
                                                     2,
                                                     std::chrono::seconds(1));
    }

    virtual void
    TearDown()
    {
        mds_manager_.reset();
        VolManagerTestSetup::TearDown();
    }

    MDSNodeConfigs
    node_configs()
    {
        return node_configs(*mds_manager_);
    }

    MDSNodeConfigs
    node_configs(mds::Manager& mgr)
    {
        const mds::ServerConfigs scfgs(mgr.server_configs());

        EXPECT_EQ(2U,
                  scfgs.size());

        MDSNodeConfigs ncfgs;
        ncfgs.reserve(scfgs.size());

        for (const auto& s : scfgs)
        {
            ncfgs.emplace_back(s.node_config);
        }

        return ncfgs;
    }

    std::unique_ptr<MetaDataBackendConfig>
    metadata_backend_config()
    {
        return metadata_backend_config(*mds_manager_);
    }

    std::unique_ptr<MetaDataBackendConfig>
    metadata_backend_config(mds::Manager& mgr)
    {
        return std::unique_ptr<MetaDataBackendConfig>(new MDSMetaDataBackendConfig(node_configs(mgr),
                                                                                   ApplyRelocationsToSlaves::T));
    }

    VanillaVolumeConfigParameters
    make_volume_params(const be::BackendTestSetup::WithRandomNamespace& wrns)
    {
        const VolumeSize vsize(CachePage::capacity() *
                               10 *
                               default_cluster_size());

        return VanillaVolumeConfigParameters(VolumeId(wrns.ns().str()),
                                             wrns.ns(),
                                             vsize,
                                             new_owner_tag())
            .metadata_backend_config(metadata_backend_config())
            .metadata_cache_capacity(1);
    }

    Volume*
    make_volume(const be::BackendTestSetup::WithRandomNamespace& wrns)
    {
        return newVolume(make_volume_params(wrns));
    }

    void
    check_config(Volume& v,
                 const std::vector<MDSNodeConfig>& ncfgs,
                 bool expect_failover)
    {
        const VolumeConfig vcfg(v.get_config());

        ASSERT_EQ(MetaDataBackendType::MDS,
                  vcfg.metadata_backend_config_->backend_type());

        auto mcfg =
            dynamic_cast<const MDSMetaDataBackendConfig*>(vcfg.metadata_backend_config_.get());
        if (expect_failover)
        {
            ASSERT_EQ(ncfgs[0], mcfg->node_configs()[1]);
            ASSERT_EQ(ncfgs[1], mcfg->node_configs()[0]);
        }
        else
        {
            ASSERT_EQ(ncfgs[0], mcfg->node_configs()[0]);
            ASSERT_EQ(ncfgs[1], mcfg->node_configs()[1]);
        }
    }

    void
    test_before_tlog(bool failover)
    {
        const auto wrns(make_random_namespace());

        Volume* v = make_volume(*wrns);

        const auto ncfgs(node_configs());
        const std::string pattern("King Leer");

        {
            SCOPED_BLOCK_BACKEND(v);

            writeToVolume(v,
                          v->getClusterMultiplier() * CachePage::capacity(),
                          v->getClusterSize(),
                          pattern);

            if (failover)
            {
                mds_manager_->stop_one(ncfgs[0]);

                checkVolume(v,
                            0,
                            v->getClusterSize(),
                            "");
            }
            else
            {
                const std::vector<MDSNodeConfig> ncfgs2{ ncfgs[1],
                                                         ncfgs[0] };

                v->updateMetaDataBackendConfig(MDSMetaDataBackendConfig(ncfgs2,
                                                                        ApplyRelocationsToSlaves::T));
            }

            check_config(*v,
                         ncfgs,
                         true);

            checkVolume(v,
                        v->getClusterMultiplier() * CachePage::capacity(),
                        v->getClusterSize(),
                        pattern);
        }

        destroyVolume(v,
                      DeleteLocalData::F,
                      RemoveVolumeCompletely::F);

        v = localRestart(wrns->ns());

        check_config(*v,
                     ncfgs,
                     true);

        checkVolume(v,
                    v->getClusterMultiplier() * CachePage::capacity(),
                    v->getClusterSize(),
                    pattern);
    }

    void
    test_after_tlog(bool failover)
    {
        const auto wrns(make_random_namespace());

        Volume* v = make_volume(*wrns);

        const std::string pattern1("Hairdresser On Fire");

        {
            SCOPED_BLOCK_BACKEND(v);

            writeToVolume(v,
                          v->getClusterMultiplier() * CachePage::capacity(),
                          v->getClusterSize(),
                          pattern1);
        }

        v->scheduleBackendSync();
        waitForThisBackendWrite(v);

        const std::string pattern2("Such A Little Thing Makes Such A Big Difference");

        writeToVolume(v,
                      2 * v->getClusterMultiplier() * CachePage::capacity(),
                      v->getClusterSize(),
                      pattern2);

        const auto ncfgs(node_configs());

        if (failover)
        {
            mds_manager_->stop_one(ncfgs[0]);

            checkVolume(v,
                        0,
                        v->getClusterSize(),
                        "");
        }
        else
        {
            const std::vector<MDSNodeConfig> ncfgs2{ ncfgs[1],
                                                     ncfgs[0] };

            v->updateMetaDataBackendConfig(MDSMetaDataBackendConfig(ncfgs2,
                                                                    ApplyRelocationsToSlaves::T));
        }

        check_config(*v,
                     ncfgs,
                     true);

        checkVolume(v,
                    v->getClusterMultiplier() * CachePage::capacity(),
                    v->getClusterSize(),
                    pattern1);

        checkVolume(v,
                    2 * v->getClusterMultiplier() * CachePage::capacity(),
                    v->getClusterSize(),
                    pattern2);

        destroyVolume(v,
                      DeleteLocalData::F,
                      RemoveVolumeCompletely::F);

        v = localRestart(wrns->ns());

        check_config(*v,
                     ncfgs,
                     true);

        checkVolume(v,
                    v->getClusterMultiplier() * CachePage::capacity(),
                    v->getClusterSize(),
                    pattern1);

        checkVolume(v,
                    2 * v->getClusterMultiplier() * CachePage::capacity(),
                    v->getClusterSize(),
                    pattern2);
    }

    TODO("AR: fix CachedMetaDataStore::compare instead!?");
    void
    check_metadata(Volume& v,
                   MetaDataStoreInterface& mdi)
    {
        for (ClusterAddress ca = 0; ca < v.getSize() / v.getClusterSize(); ++ca)
        {
            ClusterLocationAndHash master;
            v.getMetaDataStore()->readCluster(ca, master);

            ClusterLocationAndHash slave;
            mdi.readCluster(ca, slave);

            ASSERT_EQ(master.clusterLocation, slave.clusterLocation);
            ASSERT_EQ(master.weed, slave.weed);
        }
    }

    void
    check_slave_sync(Volume& v,
                     const be::BackendTestSetup::WithRandomNamespace& wrns,
                     const MDSNodeConfig& cfg,
                     bool wait = true)
    {
        MetaDataBackendInterfacePtr mdb(std::make_shared<MDSMetaDataBackend>(cfg,
                                                                             wrns.ns(),
                                                                             boost::none));

        const uint32_t secs = mds_manager_->poll_interval().count();
        const uint32_t sleep_msecs = 100;
        const uint32_t count = wait ? 3 * 1000 * secs / sleep_msecs : 1;

        for (size_t i = 0; i < count; ++i)
        {
            if (mdb->lastCorkUUID() == v.getMetaDataStore()->lastCork())
            {
                CachedMetaDataStore md(mdb,
                                       "slave");
                check_metadata(v,
                               md);
                return;
            }
            else
            {
                boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_msecs));
            }
        }

        FAIL() << "slave not in sync after " << (count * sleep_msecs) << " milliseconds";
    }

    void
    worker(Volume& v,
           std::atomic<bool>& stop)
    {
        size_t worker_iterations = 0;

        auto make_pattern([](size_t i,
                             size_t c) -> std::string
                          {
                              return "iteration-"s +
                                  boost::lexical_cast<std::string>(i) +
                                  "-cluster-"s +
                                  boost::lexical_cast<std::string>(c);
                          });

        const size_t csize = v.getClusterSize();
        const size_t clusters = v.getSize() < csize;

        while (not stop)
        {
            for (size_t i = 0; i < clusters; ++i)
            {
                writeToVolume(&v,
                              i * v.getClusterMultiplier(),
                              csize,
                              make_pattern(worker_iterations,
                                           i));
            }

            v.scheduleBackendSync();

            for (size_t i = 0; i < clusters; ++i)
            {
                checkVolume(&v,
                            i * v.getClusterMultiplier(),
                            csize,
                            make_pattern(worker_iterations,
                                         i));
            }

            ++worker_iterations;
        }

        LOG_INFO("worker exiting after " << worker_iterations << " iterations");
    }

    using MonkeyFun = std::function<void(Volume&, std::atomic<bool>&)>;

    void
    monkey_business(const boost::chrono::seconds& duration,
                    MonkeyFun monkey)
    {
        const auto wrns(make_random_namespace());
        Volume* v = make_volume(*wrns);
        std::atomic<bool> stop(false);

        auto wfuture(std::async(std::launch::async,
                                [&]
                                {
                                    worker(*v,
                                           stop);
                                }));

        auto mfuture(std::async(std::launch::async,
                                [&]
                                {
                                    monkey(*v,
                                           stop);
                                }));

        boost::this_thread::sleep_for(duration);

        stop = true;

        wfuture.wait();
        mfuture.wait();
    }

    scrubbing::ScrubReply
    scrub(const scrubbing::ScrubWork& work,
          double fill_ratio = 1.0)
    {
        return scrubbing::ScrubberAdapter::scrub(work,
                                                 getTempPath(testName_),
                                                 5, // region_size_exponent
                                                 fill_ratio, // fill ratio
                                                 false, // apply immediately
                                                 true); // verbose
    }

    scrubbing::ScrubberResult
    get_scrub_result(Volume& v,
                     const scrubbing::ScrubReply& rep)
    {
        EXPECT_EQ(rep.ns_.str(),
                  v.getNamespace().str());

        fs::path p(FileUtils::temp_path() / rep.scrub_result_name_);
        ALWAYS_CLEANUP_FILE(p);

        v.getBackendInterface()->read(p,
                                      rep.scrub_result_name_,
                                      InsistOnLatestVersion::T);

        scrubbing::ScrubberResult scrub_result;
        fs::ifstream ifs(p);
        boost::archive::text_iarchive ia(ifs);
        ia >> scrub_result;

        return scrub_result;
    }

    const scrubbing::ScrubReply
    prepare_scrub_test(Volume& v,
                       const std::string& fst_cluster_pattern = "first cluster",
                       const std::string& snd_cluster_pattern = "second cluster")
    {
        const size_t wsize(v.getClusterSize());

        writeToVolume(&v,
                      0,
                      wsize,
                      fst_cluster_pattern);

        const std::string tmp_pattern(fst_cluster_pattern +
                                      " but this will be overwritten and scrubbed away");
        const size_t nclusters = v.getSCOMultiplier();

        for (size_t i = 0; i < nclusters; ++i)
        {
            writeToVolume(&v,
                          v.getClusterMultiplier(),
                          wsize,
                          tmp_pattern);
        }

        const SnapshotName snap1("snap1-"s + yt::UUID().str());
        v.createSnapshot(snap1);
        waitForThisBackendWrite(&v);

        writeToVolume(&v,
                      v.getClusterMultiplier(),
                      wsize,
                      snd_cluster_pattern);

        const SnapshotName snap2("snap2-"s + yt::UUID().str());
        v.createSnapshot(snap2);
        waitForThisBackendWrite(&v);

        v.deleteSnapshot(snap1);
        waitForThisBackendWrite(&v);

        const std::vector<scrubbing::ScrubWork>
            scrub_work(v.getScrubbingWork(boost::none,
                                          snap2));

        EXPECT_EQ(1U, scrub_work.size());

        auto scrub_reply(scrub(scrub_work[0]));

        const auto scrub_res(get_scrub_result(v,
                                              scrub_reply));

        EXPECT_FALSE(scrub_res.relocs.empty());

        return scrub_reply;
    }

    uint64_t
    catch_up(const MDSNodeConfig& cfg,
             const std::string& nspace,
             DryRun dry_run)
    {
        mds::ClientNG::Ptr client(mds::ClientNG::create(cfg));
        mds::TableInterfacePtr table(client->open(nspace));
        return table->catch_up(dry_run);
    }

    using RelocMap = std::map<ClusterAddress, ClusterLocation>;

    RelocMap
    reloc_map(Volume& v,
              const scrubbing::ScrubReply& scrub_reply)
    {
        const auto scrub_res(get_scrub_result(v,
                                              scrub_reply));

        std::map<ClusterAddress, ClusterLocation> relocmap;

        auto treader(makeCombinedTLogReader(getTempPath(testName_).string(),
                                            scrub_res.relocs,
                                            v.getBackendInterface()->clone()));

        const Entry* e = nullptr;
        while ((e = treader->nextLocation()))
        {
            const Entry* f = treader->nextLocation();

            EXPECT_TRUE(f != nullptr);
            EXPECT_EQ(e->clusterAddress(),
                      f->clusterAddress());

            auto r(relocmap.insert(std::make_pair(f->clusterAddress(),
                                                  f->clusterLocation())));
            EXPECT_TRUE(r.second);
        }

        return relocmap;
    }

    void
    check_reloc_map(Volume& v,
                    const RelocMap& relocmap)
    {
        MetaDataStoreInterface& md = *v.getMetaDataStore();
        for (const auto& p : relocmap)
        {
            ClusterLocationAndHash clh;
            md.readCluster(p.first,
                           clh);
            ASSERT_EQ(p.second,
                      clh.clusterLocation);
        }
    }

    std::unique_ptr<mds::Manager> mds_manager_;
};

TEST_P(MDSVolumeTest, construction)
{
    const auto wrns(make_random_namespace());

    Volume* v = make_volume(*wrns);

    check_config(*v,
                 node_configs(),
                 false);
}

TEST_P(MDSVolumeTest, failover_on_construction)
{
    const auto wrns(make_random_namespace());
    const auto params(make_volume_params(*wrns));
    const auto ncfgs(node_configs());

    mds_manager_->stop_one(ncfgs[0]);

    Volume* v = newVolume(params);

    check_config(*v,
                 ncfgs,
                 true);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    v = localRestart(wrns->ns());

    check_config(*v,
                 ncfgs,
                 true);
}

TEST_P(MDSVolumeTest, failed_construction)
{
    const auto wrns(make_random_namespace());
    const auto params(make_volume_params(*wrns));

    const MDSNodeConfigs ncfgs(node_configs());

    mds_manager_->stop_one(ncfgs[0]);
    mds_manager_->stop_one(ncfgs[1]);

    EXPECT_THROW(newVolume(params),
                 std::exception);
}

TEST_P(MDSVolumeTest, broken_config_update)
{
    const auto wrns(make_random_namespace());
    Volume* v = make_volume(*wrns);

    // MDSMetaDataBackend mdb(mds2_->node_configs()[0],
    //                        wrns->ns());

    const std::string pattern("test");
    const size_t wsize(v->getClusterSize() * CachePage::capacity() * 3);

    writeToVolume(v,
                  0,
                  wsize,
                  pattern);

    const TCBTMetaDataBackendConfig tcfg;
    ASSERT_THROW(v->updateMetaDataBackendConfig(tcfg),
                 std::exception);

    const auto cfg(v->getMetaDataStore()->getBackendConfig());
    const auto mcfg(dynamic_cast<const MDSMetaDataBackendConfig*>(cfg.get()));

    EXPECT_TRUE(mcfg != nullptr);

    const MDSNodeConfigs& ncfgs1(mcfg->node_configs());
    const MDSNodeConfigs ncfgs2(node_configs());

    ASSERT_EQ(ncfgs1,
              ncfgs2);

    checkVolume(v,
                0,
                wsize,
                pattern);
}

TEST_P(MDSVolumeTest, sole_mds_temporarily_gone)
{
    const auto wrns(make_random_namespace());
    Volume* v = make_volume(*wrns);

    const mds::ServerConfigs scfgs(mds_manager_->server_configs());
    mds_manager_->stop_one(scfgs[1].node_config);

    const MDSNodeConfigs cfg{ scfgs[0].node_config };
    v->updateMetaDataBackendConfig(MDSMetaDataBackendConfig(cfg,
                                                            ApplyRelocationsToSlaves::T));

    const std::string pattern("test");
    const size_t wsize(v->getClusterSize());

    writeToVolume(v,
                  0,
                  wsize,
                  pattern);

    const SnapshotName snap("snap");
    v->createSnapshot(snap);
    waitForThisBackendWrite(v);

    const std::string pattern2("test2");

    writeToVolume(v,
                  0,
                  wsize,
                  pattern2);

    checkVolume(v,
                0,
                wsize,
                pattern2);

    mds_manager_->stop_one(scfgs[0].node_config);
    mds_manager_->start_one(scfgs[0]);

    restoreSnapshot(v,
                    snap);

    checkVolume(v,
                0,
                wsize,
                pattern);
}

TEST_P(MDSVolumeTest, slave_catchup)
{
    const auto wrns(make_random_namespace());
    Volume* v = make_volume(*wrns);

    const MDSNodeConfigs ncfgs(node_configs());
    MDSMetaDataBackend mdb(ncfgs[1],
                           wrns->ns(),
                           boost::none);

    EXPECT_EQ(boost::none,
              mdb.lastCorkUUID());

    const std::string pattern1("first");
    writeToVolume(v,
                  v->getClusterMultiplier(),
                  v->getClusterSize(),
                  pattern1);

    const SnapshotName snap1("first-snap");
    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);

    check_slave_sync(*v,
                     *wrns,
                     ncfgs[1]);

    const std::string pattern2("second");
    writeToVolume(v,
                  2 * v->getClusterMultiplier(),
                  v->getClusterSize(),
                  pattern2);

    const SnapshotName snap2("second-snap");
    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);

    check_slave_sync(*v,
                     *wrns,
                     ncfgs[1]);

    v->restoreSnapshot(snap1);

    check_slave_sync(*v,
                     *wrns,
                     ncfgs[1]);
}

TEST_P(MDSVolumeTest, failover_before_tlog)
{
    test_before_tlog(true);
}

TEST_P(MDSVolumeTest, reconfigure_before_tlog)
{
    test_before_tlog(false);
}

TEST_P(MDSVolumeTest, failover_after_tlog)
{
    test_after_tlog(true);
}

TEST_P(MDSVolumeTest, reconfigure_after_tlog)
{
    test_after_tlog(false);
}

TEST_P(MDSVolumeTest, catch_up)
{
    const size_t num_tlogs = 7;

    const auto wrns(make_random_namespace());
    Volume* v = make_volume(*wrns);

    std::unique_ptr<mds::Manager>
        mds_manager(mds_test_setup_->make_manager(cm_,
                                                  1,
                                                  std::chrono::seconds(7200)));

    const mds::ServerConfigs scfgs(mds_manager->server_configs());
    ASSERT_EQ(1U,
              scfgs.size());

    mds::ClientNG::Ptr client(mds::ClientNG::create(scfgs[0].node_config));

    mds::TableInterfacePtr table(client->open(wrns->ns().str()));

    const size_t csize = v->getClusterSize();
    ASSERT_LT(num_tlogs * CachePage::capacity() * csize,
              v->getSize());

    for (size_t i = 0; i < num_tlogs; ++i)
    {
        const auto pattern(boost::lexical_cast<std::string>(i));
        writeToVolume(v,
                      i * v->getClusterMultiplier() * CachePage::capacity(),
                      csize,
                      pattern);
        scheduleBackendSync(v);
    }

    waitForThisBackendWrite(v);

    EXPECT_EQ(num_tlogs,
              v->getSnapshotManagement().getTLogsWrittenToBackend().size());

    EXPECT_EQ(num_tlogs,
              table->catch_up(DryRun::T));

    EXPECT_EQ(num_tlogs,
              table->catch_up(DryRun::T));

    EXPECT_EQ(num_tlogs,
              table->catch_up(DryRun::F));

    EXPECT_EQ(0U,
              table->catch_up(DryRun::F));

    EXPECT_EQ(0U,
              table->catch_up(DryRun::T));

    EXPECT_TRUE(mds::Role::Slave == table->get_role());

    const MDSNodeConfigs ncfgs{ scfgs[0].node_config };
    v->updateMetaDataBackendConfig(MDSMetaDataBackendConfig(ncfgs,
                                                            ApplyRelocationsToSlaves::T));

    const MDSNodeConfigs ncfgs2(node_configs());
    mds_manager_->stop_one(ncfgs2[0]);
    mds_manager_->stop_one(ncfgs2[1]);

    for (size_t i = 0; i < num_tlogs; ++i)
    {
        const auto pattern(boost::lexical_cast<std::string>(i));
        checkVolume(v,
                    i * v->getClusterMultiplier() * CachePage::capacity(),
                    csize,
                    pattern);
    }

    EXPECT_TRUE(mds::Role::Master == table->get_role());

    EXPECT_EQ(0U,
              table->catch_up(DryRun::F));

    EXPECT_EQ(0U,
              table->catch_up(DryRun::T));
}

TEST_P(MDSVolumeTest, failover_monkey_business)
{
    auto f([&](Volume& v,
               std::atomic<bool>& stop)
           {
               size_t monkey_iterations = 0;
               size_t failovers = 0;
               yt::SourceOfUncertainty rand;

               const mds::ServerConfigs scfgs(mds_manager_->server_configs());
               mds::ServerConfig scfg1(scfgs[0]);
               mds::ServerConfig scfg2(scfgs[1]);

               while (not stop)
               {
                   const auto cfg(v.getMetaDataStore()->getBackendConfig());
                   const auto
                       mcfg(dynamic_cast<const MDSMetaDataBackendConfig*>(cfg.get()));

                   EXPECT_TRUE(mcfg != nullptr);

                   const auto& ncfgs(mcfg->node_configs());

                   LOG_INFO("expected primary MDS: " << scfg1.node_config <<
                            ", reported primary MDS: " << ncfgs[0]);

                   if (ncfgs[0] != scfg1.node_config)
                   {
                       EXPECT_EQ(2U,
                                 ncfgs.size());

                       EXPECT_EQ(scfg1.node_config,
                                 ncfgs[1]);

                       boost::this_thread::sleep_for(boost::chrono::seconds(1));
                   }
                   else
                   {
                       const mds::ServerConfig
                           scfg(mds_test_setup_->next_server_config());
                       mds_manager_->start_one(scfg);
                       auto client(mds::ClientNG::create(scfg.node_config));
                       client->open(v.getNamespace().str());

                       LOG_INFO("new slave: " << scfg);

                       const uint64_t msecs = rand(0ULL, 2000ULL);
                       boost::this_thread::sleep_for(boost::chrono::milliseconds(msecs));

                       const std::vector<MDSNodeConfig> cfgs{ scfg1.node_config,
                                                              scfg.node_config };
                       v.updateMetaDataBackendConfig(MDSMetaDataBackendConfig(cfgs,
                                                                              ApplyRelocationsToSlaves::T));
                       mds_manager_->stop_one(scfg1.node_config);
                       scfg1 = scfg;
                       ++failovers;
                   }

                   ++monkey_iterations;
               }

               LOG_INFO("monkey exiting after " << monkey_iterations <<
                        " iterations, " << failovers << " failovers");

               EXPECT_LT(0U,
                         failovers);
           });

    monkey_business(boost::chrono::seconds(30),
                    std::move(f));
}

TEST_P(MDSVolumeTest, migration_monkey_business)
{
    auto f([&](Volume& v,
               std::atomic<bool>& stop)
           {
               size_t monkey_iterations = 0;
               yt::SourceOfUncertainty rand;

               while (not stop)
               {
                   const uint64_t msecs = rand(500ULL, 2500ULL);
                   boost::this_thread::sleep_for(boost::chrono::milliseconds(msecs));

                   const auto cfg(v.getMetaDataStore()->getBackendConfig());
                   const auto
                       mcfg(dynamic_cast<const MDSMetaDataBackendConfig*>(cfg.get()));

                   EXPECT_TRUE(mcfg != nullptr);

                   const auto& ncfgs(mcfg->node_configs());
                   EXPECT_EQ(2U, ncfgs.size());

                   const std::vector<MDSNodeConfig> cfgs{ ncfgs[1],
                           ncfgs[0] };

                   v.updateMetaDataBackendConfig(MDSMetaDataBackendConfig(cfgs,
                                                                          ApplyRelocationsToSlaves::T));
                   ++monkey_iterations;
               }

               LOG_INFO("monkey exiting after " << monkey_iterations <<
                        " iterations");
           });

    monkey_business(boost::chrono::seconds(30),
                    std::move(f));
}

TEST_P(MDSVolumeTest, master_gone_and_no_slaves)
{
    const size_t iterations = 7;

    yt::SourceOfUncertainty rand;

    for (size_t i = 0; i < iterations; ++i)
    {
        const auto wrns(make_random_namespace());
        Volume* v = make_volume(*wrns);

        auto on_exit(yt::make_scope_exit([&]
                                         {
                                             destroyVolume(v,
                                                           DeleteLocalData::T,
                                                           RemoveVolumeCompletely::T);
                                         }));

        const mds::ServerConfig scfg(mds_test_setup_->next_server_config());
        mds_manager_->start_one(scfg);

        const std::vector<MDSNodeConfig> ncfgs{ scfg.node_config };
        v->updateMetaDataBackendConfig(MDSMetaDataBackendConfig(ncfgs,
                                                                ApplyRelocationsToSlaves::T));

        std::atomic<bool> stop(false);

        auto future(std::async(std::launch::async,
                               [&]
                               {
                                   EXPECT_THROW(worker(*v,
                                                       stop),
                                                std::exception);
                                   EXPECT_TRUE(v->is_halted());
                               }));

        const uint64_t msecs = rand(500ULL, 5000ULL);
        boost::this_thread::sleep_for(boost::chrono::milliseconds(msecs));

        LOG_INFO("Shooting down " << scfg.node_config);

        mds_manager_->stop_one(scfg.node_config);

        future.wait();
    }
}

TEST_P(MDSVolumeTest, futile_scrub)
{
    const auto wrns(make_random_namespace());
    Volume* v = make_volume(*wrns);

    const MDSNodeConfigs ncfgs(node_configs());
    MDSMetaDataBackend mdb(ncfgs[1],
                           wrns->ns(),
                           boost::none);

    const auto old_scrub_id(v->getMetaDataStore()->scrub_id());

    const size_t wsize(v->getClusterSize() * CachePage::capacity() * 2);
    ASSERT_LE(2 * wsize, v->getSize());

    const std::string pattern1("one");

    writeToVolume(v,
                  0,
                  wsize,
                  pattern1);

    const SnapshotName snap1("snap1");
    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);

    const std::string pattern2("two");

    writeToVolume(v,
                  wsize / v->getLBASize(),
                  wsize,
                  pattern2);

    const SnapshotName snap2("snap2");
    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);

    v->deleteSnapshot(snap1);
    waitForThisBackendWrite(v);

    const std::vector<scrubbing::ScrubWork>
        scrub_work(v->getScrubbingWork(boost::none,
                                       snap2));

    ASSERT_EQ(1U, scrub_work.size());

    auto scrub_reply(scrub(scrub_work[0],
                           0.0));

    const auto scrub_res(get_scrub_result(*v,
                                          scrub_reply));

    ASSERT_TRUE(scrub_res.relocs.empty());

    apply_scrub_reply(*v,
                      scrub_reply);

    const auto new_scrub_id(v->getMetaDataStore()->scrub_id());

    EXPECT_NE(old_scrub_id,
              new_scrub_id);

    EXPECT_EQ(new_scrub_id,
              mdb.scrub_id());

    check_slave_sync(*v,
                     *wrns,
                     ncfgs[1],
                     false);
}

TEST_P(MDSVolumeTest, happy_scrub)
{
    const auto wrns(make_random_namespace());
    Volume* v = make_volume(*wrns);

    const auto old_scrub_id(v->getMetaDataStore()->scrub_id());

    const MDSNodeConfigs ncfgs(node_configs());
    MDSMetaDataBackend mdb(ncfgs[1],
                           wrns->ns(),
                           boost::none);

    const scrubbing::ScrubReply scrub_reply(prepare_scrub_test(*v));

    apply_scrub_reply(*v,
                      scrub_reply);

    const auto new_scrub_id(v->getMetaDataStore()->scrub_id());

    EXPECT_NE(old_scrub_id,
              new_scrub_id);

    EXPECT_EQ(new_scrub_id,
              mdb.scrub_id());

    check_slave_sync(*v,
                     *wrns,
                     ncfgs[1],
                     false);
}

TEST_P(MDSVolumeTest, scrub_with_master_out_to_lunch)
{
    const auto wrns(make_random_namespace());
    Volume* v = make_volume(*wrns);

    const auto old_scrub_id(v->getMetaDataStore()->scrub_id());

    const std::string pattern1("cluster #1");
    const std::string pattern2("cluster #2");

    const scrubbing::ScrubReply scrub_reply(prepare_scrub_test(*v,
                                                               pattern1,
                                                               pattern2));

    const auto relocmap(reloc_map(*v,
                                  scrub_reply));

    ASSERT_FALSE(relocmap.empty());

    const MDSNodeConfigs ncfgs(node_configs());
    mds_manager_->stop_one(ncfgs[0]);

    apply_scrub_reply(*v,
                      scrub_reply);

    const auto new_scrub_id(v->getMetaDataStore()->scrub_id());
    EXPECT_NE(old_scrub_id,
              new_scrub_id);
}

TEST_P(MDSVolumeTest, scrub_with_slave_out_to_lunch)
{
    const auto wrns(make_random_namespace());
    Volume* v = make_volume(*wrns);

    const auto old_scrub_id(v->getMetaDataStore()->scrub_id());

    const std::string pattern1("cluster #1");
    const std::string pattern2("cluster #2");

    const scrubbing::ScrubReply scrub_reply(prepare_scrub_test(*v,
                                                               pattern1,
                                                               pattern2));

    const auto scrub_res(get_scrub_result(*v,
                                          scrub_reply));

    const auto relocmap(reloc_map(*v,
                                  scrub_reply));
    ASSERT_FALSE(relocmap.empty());

    const mds::ServerConfigs scfgs(mds_manager_->server_configs());

    catch_up(scfgs[1].node_config,
             wrns->ns().str(),
             DryRun::F);

    mds_manager_->stop_one(scfgs[1].node_config);

    apply_scrub_reply(*v,
                      scrub_reply);

    check_reloc_map(*v,
                    relocmap);

    const auto new_scrub_id(v->getMetaDataStore()->scrub_id());

    mds_manager_->start_one(scfgs[1]);
    mds_manager_->stop_one(scfgs[0].node_config);

    check_reloc_map(*v,
                    relocmap);

    checkVolume(v,
                0,
                v->getClusterSize(),
                pattern1);

    checkVolume(v,
                v->getClusterMultiplier(),
                v->getClusterSize(),
                pattern2);

    EXPECT_NE(old_scrub_id, new_scrub_id);

    EXPECT_EQ(new_scrub_id,
              v->getMetaDataStore()->scrub_id());
}

TEST_P(MDSVolumeTest, failover_performance)
{
    const uint32_t tlogs_total =
        yt::System::get_env_with_default("MDS_FAILOVER_PERF_TLOGS_TOTAL",
                                         2U);

    const uint32_t tlogs_sync =
        yt::System::get_env_with_default("MDS_FAILOVER_PERF_TLOGS_SYNC",
                                         1U);

    ASSERT_LE(tlogs_sync,
              tlogs_total) << "fix your test";

    const auto
        poll_secs(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::hours(1)));
    mds_manager_ =
        mds_test_setup_->make_manager(cm_,
                                      2,
                                      poll_secs);

    const MDSNodeConfigs ncfgs(node_configs());
    ASSERT_EQ(2U,
              ncfgs.size());

    const auto wrns(make_random_namespace());
    Volume* v = make_volume(*wrns);

    const uint32_t clusters = v->getSnapshotManagement().maxTLogEntries();

    for (uint32_t i = 0; i < tlogs_total; ++i)
    {
        writeClusters(v,
                      clusters,
                      "some really important data");
        if (i == tlogs_sync - 1)
        {
            waitForThisBackendWrite(v);

            catch_up(ncfgs.back(),
                     wrns->ns().str(),
                     DryRun::F);
        }
    }

    waitForThisBackendWrite(v);
    const std::vector<MDSNodeConfig> new_ncfgs{ ncfgs.back() };

    yt::wall_timer w;
    v->updateMetaDataBackendConfig(MDSMetaDataBackendConfig(new_ncfgs,
                                                            ApplyRelocationsToSlaves::T));

    double t = w.elapsed();

    std::cout << "clusters / TLog: " << clusters <<
        ", total TLogs: " << tlogs_total <<
        ", TLogs in sync: " << tlogs_sync <<
        ", replay duration: " << t <<
        " seconds -> " << ((tlogs_total - tlogs_sync) / t) <<
        " TLogs / second" <<
        std::endl;
}

TEST_P(MDSVolumeTest, no_relocations_on_slaves)
{
    const auto wrns(make_random_namespace());
    Volume* v = make_volume(*wrns);

    std::unique_ptr<mds::Manager>
        mgr(mds_test_setup_->make_manager(cm_,
                                          2,
                                          std::chrono::seconds(3600)));

    auto check([&](ApplyRelocationsToSlaves apply_relocs)
    {
        v->updateMetaDataBackendConfig(MDSMetaDataBackendConfig(node_configs(*mgr),
                                                                apply_relocs));

        std::unique_ptr<MetaDataBackendConfig>
            mcfg(v->getMetaDataStore()->getBackendConfig());
        auto mdscfg = dynamic_cast<MDSMetaDataBackendConfig*>(mcfg.get());

        ASSERT_TRUE(nullptr != mdscfg);
        EXPECT_EQ(apply_relocs,
                  mdscfg->apply_relocations_to_slaves());

        const auto old_scrub_id(v->getMetaDataStore()->scrub_id());

        const scrubbing::ScrubReply scrub_reply(prepare_scrub_test(*v));

        apply_scrub_reply(*v,
                          scrub_reply);

        const auto new_scrub_id(v->getMetaDataStore()->scrub_id());
        EXPECT_NE(old_scrub_id, new_scrub_id);

        MetaDataBackendInterfacePtr
            mdb(std::make_shared<MDSMetaDataBackend>(node_configs(*mgr)[1],
                                                     wrns->ns(),
                                                     boost::none));
        const MaybeScrubId maybe_scrub_id(mdb->scrub_id());

        if (apply_relocs == ApplyRelocationsToSlaves::F)
        {
            EXPECT_EQ(boost::none,
                      maybe_scrub_id);
        }
        else
        {
            ASSERT_NE(boost::none,
                      maybe_scrub_id);
            EXPECT_EQ(new_scrub_id,
                      *maybe_scrub_id);
        }
    });

    check(ApplyRelocationsToSlaves::F);
    check(ApplyRelocationsToSlaves::T);
}

// cf. OVS-2708
TEST_P(MDSVolumeTest, local_restart_of_pristine_clone_with_empty_mds)
{
    const auto pns(make_random_namespace());
    Volume* p = make_volume(*pns);

    const std::string pattern("template content");
    writeToVolume(p,
                  0,
                  p->getClusterSize(),
                  pattern);

    waitForThisBackendWrite(p);

    p->setAsTemplate();

    const OwnerTag ctag(static_cast<uint64_t>(p->getOwnerTag()) + 1);

    destroyVolume(p,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    const auto cns(make_random_namespace());
    CloneVolumeConfigParameters
        cparams(VolumeId(cns->ns().str()),
                cns->ns(),
                pns->ns(),
                ctag);

    cparams.metadata_backend_config(metadata_backend_config());

    Volume* c = nullptr;

    {
        fungi::ScopedLock g(api::getManagementMutex());
        c = VolManager::get()->createClone(cparams,
                                           PrefetchVolumeData::F,
                                           CreateNamespace::F);
    }

    c->scheduleBackendSync();
    waitForThisBackendWrite(c);

    destroyVolume(c,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    // The MDS table was empty as the process wasn't shut down cleanly and
    // RocksDB's WAL is disabled.
    mds::ClientNG::Ptr client(mds::ClientNG::create(node_configs()[0]));
    mds::TableInterfacePtr table(client->open(cns->ns().str()));
    table->clear();
    table->set_role(mds::Role::Slave);

    localRestart(cns->ns());

    c = getVolume(VolumeId(cns->ns().str()));

    checkVolume(c,
                0,
                c->getClusterSize(),
                pattern);
}

namespace
{

const ClusterMultiplier
big_cluster_multiplier(VolManagerTestSetup::default_test_config().cluster_multiplier() * 2);

const auto big_clusters_config = VolManagerTestSetup::default_test_config()
    .cluster_multiplier(big_cluster_multiplier);

}

INSTANTIATE_TEST_CASE_P(MDSVolumeTests,
                        MDSVolumeTest,
                        ::testing::Values(volumedriver::VolManagerTestSetup::default_test_config(),
                                          big_clusters_config));

}
