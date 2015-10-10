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

#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/filesystem/fstream.hpp>
#include <boost/scope_exit.hpp>

#include <youtils/Assert.h>
#include <youtils/FileUtils.h>

#include <backend/BackendInterface.h>

#include <volumedriver/DataStoreNG.h>
#include <volumedriver/ScopedVolume.h>
#include <volumedriver/VolManager.h>
#include <volumedriver/VolumeConfig.h>
#include <volumedriver/VolumeThreadPool.h>

namespace volumedrivertest
{

using namespace initialized_params;
using namespace volumedriver;
namespace yt = youtils;

class SimpleVolumeTest
    : public VolManagerTestSetup
{
public:
    SimpleVolumeTest()
        : VolManagerTestSetup("SimpleVolumeTest")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }

    void
    with_snapshot_not_in_backend(std::function<void(Volume&,
                                                    const std::string& snap)> fun)
    {
        auto ns(make_random_namespace());
        Volume* v = newVolume(*ns);

        const std::string pattern("some data");
        writeToVolume(v,
                      0,
                      v->getClusterSize(),
                      pattern);

        const std::string snap("snap");

        SCOPED_BLOCK_BACKEND(v);

        createSnapshot(v,
                       snap);

        auto& sm = v->getSnapshotManagement();

        ASSERT_FALSE(sm.lastSnapshotOnBackend());
        ASSERT_EQ(snap,
                  sm.getLastSnapshotName());

        fun(*v,
            snap);

        const fs::path
            p(yt::FileUtils::create_temp_file_in_temp_dir("spanhosts.mlx"));
        ALWAYS_CLEANUP_FILE(p);

        sm.getSnapshotPersistor().saveToFile(p,
                                             SyncAndRename::T);
        v->getBackendInterface()->clone()->write(p,
                                                 snapshotFilename(),
                                                 OverwriteObject::T);

        fun(*v,
            snap);
    }
};

TEST_P(SimpleVolumeTest, test0)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    ASSERT_THROW(newVolume(VolumeId("volume1"),
                           ns,
			   VolumeSize(234567)),
                 fungi::IOException);
}

TEST_P(SimpleVolumeTest, testVolumePotential)
{
    uint64_t number_of_volumes =
        VolManager::get()->volumePotential(default_sco_mult(),
                                           boost::none);

    const std::string name("openvstorage-volumedrivertest-namespace");
    std::vector<std::unique_ptr<WithRandomNamespace> > nss;

    while(number_of_volumes > 0)
    {
        std::stringstream ss;
        ss << name << number_of_volumes;
        nss.push_back(make_random_namespace());

        ASSERT_NO_THROW(newVolume(ss.str(),
                                  nss.back()->ns()));

        uint64_t new_number_of_volumes =
            VolManager::get()->volumePotential(default_sco_mult(),
                                               boost::none);
        ASSERT_EQ(new_number_of_volumes,
                  --number_of_volumes);
    }

    nss.push_back(make_random_namespace());

    ASSERT_THROW(newVolume("openvstorage-volumedrivertest-namespace0",
                           nss.back()->ns()),
                 fungi::IOException);
}

TEST_P(SimpleVolumeTest, update_tlog_multiplier)
{
    auto wrns(make_random_namespace());
    Volume* v = newVolume(*wrns);

    const boost::optional<TLogMultiplier> tm(v->getTLogMultiplier());
    const TLogMultiplier tm_eff(v->getEffectiveTLogMultiplier());

    fungi::ScopedLock l(api::getManagementMutex());

    const uint64_t pot = volume_potential_sco_cache(v->getSCOMultiplier(),
                                                    tm_eff);

    ASSERT_LT(1U, pot);

    EXPECT_THROW(v->setTLogMultiplier(TLogMultiplier(tm_eff.t * (pot + 10))),
                 fungi::IOException);

    EXPECT_THROW(v->setTLogMultiplier(TLogMultiplier(0)),
                 fungi::IOException);


    const TLogMultiplier tm_new(TLogMultiplier(tm_eff.t * pot));
    EXPECT_NO_THROW(v->setTLogMultiplier(tm_new));

    EXPECT_EQ(tm_new,
              v->getTLogMultiplier());
}

TEST_P(SimpleVolumeTest, update_sco_multiplier)
{
    auto wrns(make_random_namespace());
    Volume* v = newVolume(*wrns);

    fungi::ScopedLock l(api::getManagementMutex());

    // current min: 1 MiB
    const SCOMultiplier min(256);

    EXPECT_THROW(v->setSCOMultiplier(SCOMultiplier(min.t - 1)),
                 fungi::IOException);

    v->setSCOMultiplier(min);
    EXPECT_EQ(min,
              v->getSCOMultiplier());

    // current max: 128 MiB
    const SCOMultiplier max(32768);
    EXPECT_THROW(v->setSCOMultiplier(SCOMultiplier(max.t + 1)),
                 fungi::IOException);

    const SCOMultiplier sm(SCOMultiplier(v->getSCOMultiplier().t + 1));
    v->setSCOMultiplier(sm);
    EXPECT_EQ(sm,
              v->getSCOMultiplier());
}

TEST_P(SimpleVolumeTest, alignment)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns);

    ASSERT_TRUE(__alignof__(*v) >= 4);
}

TEST_P(SimpleVolumeTest, scoped_volume_with_local_deletion)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    //    Namespace ns1;
    std::unique_ptr<VolumeConfig> cfg;
    {

         ScopedVolumeWithLocalDeletion vol (newVolume(vid1,
                                                      ns1));
         writeToVolume(vol.get(),
                       0,
                       4096,
                       "blah");
         cfg.reset(new VolumeConfig(vol->get_config()));
         WAIT_FOR_THIS_VOLUME_WRITE(vol.get());
         ASSERT_THROW(localRestart(ns1),
                      fungi::IOException);
         ASSERT_THROW(restartVolume(*cfg),
                      fungi::IOException);
    }
    ASSERT_THROW(localRestart(ns1),
                 fungi::IOException);
    ASSERT_NO_THROW(restartVolume(*cfg));
}

TEST_P(SimpleVolumeTest, scoped_volume_with_local_deletion2)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();
    //Namespace ns1;
    std::unique_ptr<VolumeConfig> cfg;
     {
         ScopedVolumeWithoutLocalDeletion vol (newVolume(vid1,
                                                         ns1));
         writeToVolume(vol.get(),
                       0,
                       4096,
                       "blah");
         cfg.reset(new VolumeConfig(vol->get_config()));

         WAIT_FOR_THIS_VOLUME_WRITE(vol.get());
         ASSERT_THROW(localRestart(ns1),
                          fungi::IOException);

         ASSERT_THROW(restartVolume(*cfg),
                      fungi::IOException);
     }
    ASSERT_THROW(restartVolume(*cfg),
                 fungi::IOException);
    ASSERT_NO_THROW(localRestart(ns1));
}

TEST_P(SimpleVolumeTest, scoped_volume_with_local_deletion3)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();
    //Namespace ns1;
    std::unique_ptr<VolumeConfig> cfg;
     {
         ScopedVolumeWithoutLocalDeletion vol (newVolume(vid1,
                                                         ns1));
         writeToVolume(vol.get(),
                       0,
                       4096,
                       "blah");
         cfg.reset(new VolumeConfig(vol->get_config()));

         WAIT_FOR_THIS_VOLUME_WRITE(vol.get());
         ASSERT_THROW(localRestart(ns1),
                          fungi::IOException);

         ASSERT_THROW(restartVolume(*cfg),
                      fungi::IOException);

         checkVolume(vol.get(),
                     0,
                     4096,
                     "blah");

         vol->set_cluster_cache_mode(ClusterCacheMode::LocationBased);
         vol->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);

         checkVolume(vol.get(),
                     0,
                     4096,
                     "blah");

         ASSERT_THROW(vol->set_cluster_cache_mode(ClusterCacheMode::ContentBased),
                      fungi::IOException);

         vol->set_cluster_cache_behaviour(ClusterCacheBehaviour::NoCache);
         vol->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnWrite);

         writeToVolume(vol.get(),
                       0,
                       4096,
                       "blahblah");

         checkVolume(vol.get(),
                     0,
                     4096,
                     "blahblah");

         vol->set_cluster_cache_behaviour(ClusterCacheBehaviour::NoCache);

         writeToVolume(vol.get(),
                       0,
                       4096,
                       "blahblah");

         checkVolume(vol.get(),
                     0,
                     4096,
                     "blahblah");
     }
    ASSERT_THROW(restartVolume(*cfg),
                 fungi::IOException);
    ASSERT_NO_THROW(localRestart(ns1));
}

TEST_P(SimpleVolumeTest, sizeTest)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();
    ASSERT_THROW(newVolume(VolumeId("volume1"),
                           ns,
                           VolumeSize(VolManager::get()->real_max_volume_size() + 1)),
                 fungi::IOException);



    ASSERT_NO_THROW(newVolume(VolumeId("volume1"),
                              ns,
                              VolManager::get()->real_max_volume_size()));
}

struct VolumeWriter
{
    VolumeWriter(Volume* v)
        :vol_(v)
        , stop_(false)
    {}

    void
    operator()()
    {
        while(not stop_)
        {
            VolManagerTestSetup::writeToVolume(vol_,0, 4096,"blah");
            usleep(100);
        }
    }

    void
    stop()
    {
        stop_ = true;
    }


    Volume* vol_;
    bool stop_;
};

// disabled as of VOLDRV-1015 due to partial pointlessness
#if 0
TEST_P(SimpleVolumeTest, DISABLED_dumpMDStore)
{
    VolumeId vid("volume1");

    Volume* v = newVolume(vid,
			  backend::Namespace());
    VolumeWriter writer(v);

    boost::thread t(boost::ref(writer));


    fs::path tmp = FileUtils::create_temp_file_in_temp_dir("mdstore_backup");
    ALWAYS_CLEANUP_FILE(tmp);

    backupMDStore(vid,
                  tmp);

    writer.stop();
    t.join();
    destroyVolume(v,
                  DeleteLocalData::T,
                  false);
}
#endif

TEST_P(SimpleVolumeTest, halted)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();
    Volume* v = newVolume(VolumeId("volume1"),
			  ns);

    const std::string pattern("blah");

    ASSERT_TRUE(v);
    writeToVolume(v,
                  0,
                  4096,
                  pattern);

    checkVolume(v,
                0,
                4096,
                pattern);

    v->halt();

    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "fubar"),
                 std::exception);

    EXPECT_THROW(checkVolume(v,
                             0,
                             4096,
                             pattern),
                 std::exception);
}

TEST_P(SimpleVolumeTest, test1)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns);
    ASSERT_TRUE(v);
    writeToVolume(v,
                  0,
                  4096,
                  "immanuel");
    writeToVolume(v,
                  4096,
                  4096,
                  "joost");

    writeToVolume(v,
                  2*4096,
                  4096,
                  "arne");

    checkVolume(v,
                0,
                4096,
                "immanuel");
    checkVolume(v,
                4096,
                4096,
                "joost");
    checkVolume(v,
                2*4096,
                4096,
                "arne");
}

TEST_P(SimpleVolumeTest, test2)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns);
    ASSERT_TRUE(v);
    writeToVolume(v,
                  0,
                  4096,
                  "immanuel");
    writeToVolume(v,
                  0,
                  4096,
                  "joost");

    writeToVolume(v,
                  0,
                  4096,
                  "arne");

    checkVolume(v,
                0,
                4096,
                "arne");

}

TEST_P(SimpleVolumeTest, test3)
{
    // backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    Volume* v = newVolume(VolumeId("volume1"),
                          ns);

    ASSERT_TRUE(v);

    writeToVolume(v,
                  0,
                  4096,
                  "immanuel");
    v->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& ns_clone = ns1_ptr->ns();

    // backend::Namespace ns_clone;

    Volume* c = 0;
    c = createClone("clone1",
                    ns_clone,
                    ns,
                    "snap1");

    checkVolume(c,
                0,
                4096,
                "immanuel");
}

TEST_P(SimpleVolumeTest, test4)
{
    Volume* v = 0;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    //backend::Namespace ns1;
    v = newVolume(VolumeId("volume1"),
                  ns1);
    ASSERT_TRUE(v);
    writeToVolume(v,
                  0,
                  4096,
                  "a");

    v->createSnapshot(SnapshotName("snap1"));

    waitForThisBackendWrite(v);
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& ns_clone1 = ns1_ptr->ns();

    // backend::Namespace ns_clone1;

    Volume* c1 = 0;
    c1 = createClone("clone1",
                     ns_clone1,
                     ns1,
                    "snap1");
    writeToVolume(c1,
                  4096,
                  4096,
                  "b");

    checkVolume(c1,
                0,
                4096,
                "a");


    checkVolume(c1,
                4096,
                4096,
                "b");

    c1->createSnapshot(SnapshotName("snap2"));

    waitForThisBackendWrite(c1);
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns_clone2 = ns2_ptr->ns();

    // backend::Namespace ns_clone2;
    Volume* c2 = 0;
    c2 = createClone("clone2",
                     ns_clone2,
                     ns_clone1,
                     "snap2");
    ASSERT_TRUE(c2);

     checkVolume(c2,
                4096,
                 4096,
                 "b");
    checkVolume(c2,
                0,
                4096,
                "a");


    writeToVolume(c2,
                  2*4096,
                  4096,
                  "c");
    checkVolume(c2,
                2*4096,
                4096,
                "c");
    c2->createSnapshot(SnapshotName("snap3"));
    waitForThisBackendWrite(c2);


    Volume* c3 = 0;
    auto ns3_ptr = make_random_namespace();

    const backend::Namespace& ns_clone3 = ns3_ptr->ns();

    // backend::Namespace ns_clone3;
    c3 = createClone("clone3",
                     ns_clone3,
                     ns_clone2,
                     "snap3");

    ASSERT_TRUE(c3);

    checkVolume(c3,
                0,
                4096,
                "a");

    checkVolume(c3,
                4096,
                4096,
                "b");
    checkVolume(c3,
                2*4096,
                4096,
                "c");
    writeToVolume(c3,
                  3*4096,
                  4096,
                  "d");

    checkVolume(c3,
                3*4096,
                4096,
                "d");

    c3->createSnapshot(SnapshotName("snap4"));
    waitForThisBackendWrite(c3);

    Volume* c4 = 0;
    auto ns4_ptr = make_random_namespace();

    const backend::Namespace& ns_clone4 = ns4_ptr->ns();

    //    backend::Namespace ns_clone4;
    c4 = createClone("clone4",
                     ns_clone4,
                     ns_clone3,
                     "snap4");
    ASSERT_TRUE(c4);
}

TEST_P(SimpleVolumeTest, switchTLog)
{
    // difftime doesn't work for me.
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns);

    std::string init_tlog = v->getSnapshotManagement().getCurrentTLogName();
    std::vector<uint8_t> v1(4096);

    uint64_t clusters   = VolManager::get()->number_of_scos_in_tlog.value() *  v->getSCOMultiplier();

    for(unsigned i = 0; i < clusters - 1; ++i) {
        writeToVolume(v, 0, 4096, &v1[0]);
    }

    ASSERT_EQ(init_tlog, v->getSnapshotManagement().getCurrentTLogName());

    writeToVolume(v, 0, 4096, &v1[0]);

    ASSERT_NE(init_tlog, v->getSnapshotManagement().getCurrentTLogName());
}

TEST_P(SimpleVolumeTest, restoreSnapshot)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    //const backend::Namespace nspace;
    Volume* v = newVolume(VolumeId("volume1"),
                          nspace);

    const int count = v->getClusterSize();

    const std::string snapshotName1("nikon");
    const std::string snapshotName2("canon");


    writeToVolume(v, 0, count, snapshotName1);
    writeToVolume(v, 0, count, snapshotName2);

    checkVolume(v, 0, count, snapshotName2);

    {
        SCOPED_BLOCK_BACKEND(v);
        ASSERT_NO_THROW(createSnapshot(v, snapshotName1));
        writeToVolume(v, 0, count, snapshotName1);

        EXPECT_THROW(createSnapshot(v, snapshotName2), PreviousSnapshotNotOnBackendException);
        writeToVolume(v,0, count,"bbbbb");

        EXPECT_THROW(restoreSnapshot(v, snapshotName1), fungi::IOException);


    }
    waitForThisBackendWrite(v);

    {
        SCOPED_BLOCK_BACKEND(v);
        ASSERT_NO_THROW(createSnapshot(v, snapshotName2));
        writeToVolume(v,0, count,"ccccc");
    }
    waitForThisBackendWrite(v);


    EXPECT_NO_THROW(restoreSnapshot(v, snapshotName1));


    checkVolume(v,0,count,snapshotName2);

    // we should be able to rewrite now:
    writeToVolume(v,0, count,"aaaaa");

    checkVolume(v,0,count,"aaaaa");

    // make sure tlogs are not leaked
    waitForThisBackendWrite(v);

    std::list<std::string> tlogsInBackend;
    backendRegex(nspace,
                "tlog_.*",
                tlogsInBackend);

    OrderedTLogNames allTLogs;
    v->getSnapshotManagement().getAllTLogs(allTLogs,
                                           AbsolutePath::F);

    for (std::list<std::string>::iterator i = tlogsInBackend.begin();
         i != tlogsInBackend.end();
         ++i) {
        for (OrderedTLogNames::iterator j = allTLogs.begin();
             j != allTLogs.end();
             ++j)
        {
            if (*i == *j) {
                *i = "";
                break;
            }
        }
    }

    for (std::list<std::string>::iterator i = tlogsInBackend.begin();
         i != tlogsInBackend.end();
         ++i)
    {
        EXPECT_EQ("", *i) << "TLog " << *i << " leaked in backend";
    }
}

TEST_P(SimpleVolumeTest, restoreSnapshot2)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          nspace);

    const int count = v->getClusterSize();

    const std::string snapshotName1("nikon");
    const std::string snapshotName2("canon");

    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);
    v->set_cluster_cache_mode(ClusterCacheMode::LocationBased);

    EXPECT_TRUE(v->isCacheOnRead());
    EXPECT_FALSE(v->isCacheOnWrite());

    writeToVolume(v, 0, count, snapshotName1);
    writeToVolume(v, 0, count, snapshotName2);

    checkVolume(v, 0, count, snapshotName2);

    {
        SCOPED_BLOCK_BACKEND(v);
        ASSERT_NO_THROW(createSnapshot(v, snapshotName1));

        v->set_cluster_cache_behaviour(ClusterCacheBehaviour::NoCache);
        EXPECT_TRUE(v->effective_cluster_cache_behaviour() == ClusterCacheBehaviour::NoCache);
        v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);
        EXPECT_TRUE(v->isCacheOnRead());
        EXPECT_FALSE(v->isCacheOnWrite());

        writeToVolume(v, 0, count, snapshotName1);

        EXPECT_THROW(createSnapshot(v, snapshotName2), PreviousSnapshotNotOnBackendException);
        writeToVolume(v,0, count,"cnanakos1");

        EXPECT_THROW(restoreSnapshot(v, snapshotName1), fungi::IOException);
    }

    waitForThisBackendWrite(v);

    {
        SCOPED_BLOCK_BACKEND(v);
        ASSERT_NO_THROW(createSnapshot(v, snapshotName2));

        v->set_cluster_cache_behaviour(ClusterCacheBehaviour::NoCache);
        EXPECT_TRUE(v->effective_cluster_cache_behaviour() == ClusterCacheBehaviour::NoCache);
        ASSERT_THROW(v->set_cluster_cache_mode(ClusterCacheMode::ContentBased),
                     fungi::IOException);
        v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);
        EXPECT_TRUE(v->effective_cluster_cache_behaviour() != ClusterCacheBehaviour::NoCache);
        EXPECT_TRUE(v->isCacheOnRead());
        EXPECT_FALSE(v->isCacheOnWrite());

        writeToVolume(v,0, count,"cnanakos2");
    }

    waitForThisBackendWrite(v);

    EXPECT_NO_THROW(restoreSnapshot(v, snapshotName1));

    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::NoCache);
    EXPECT_TRUE(v->effective_cluster_cache_behaviour() == ClusterCacheBehaviour::NoCache);
    ASSERT_THROW(v->set_cluster_cache_mode(ClusterCacheMode::ContentBased),
                 fungi::IOException);
    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);
    EXPECT_TRUE(v->isCacheOnRead());
    EXPECT_FALSE(v->isCacheOnWrite());

    checkVolume(v,0,count,snapshotName2);

    writeToVolume(v,0, count,"cnanakos4");

    checkVolume(v,0,count,"cnanakos4");

    waitForThisBackendWrite(v);

    std::list<std::string> tlogsInBackend;
    backendRegex(nspace,
                "tlog_.*",
                tlogsInBackend);

    OrderedTLogNames allTLogs;
    v->getSnapshotManagement().getAllTLogs(allTLogs,
                                           AbsolutePath::F);

    for (std::list<std::string>::iterator i = tlogsInBackend.begin();
         i != tlogsInBackend.end();
         ++i) {
        for (OrderedTLogNames::iterator j = allTLogs.begin();
             j != allTLogs.end();
             ++j)
        {
            if (*i == *j) {
                *i = "";
                break;
            }
        }
    }

    for (std::list<std::string>::iterator i = tlogsInBackend.begin();
         i != tlogsInBackend.end();
         ++i)
    {
        EXPECT_EQ("", *i) << "TLog " << *i << " leaked in backend";
    }
}

TEST_P(SimpleVolumeTest, restoreSnapshotWithFuckedUpTLogs)
{
    uint32_t scosInTlog    = 1;
    SCOMultiplier clustersInSco = SCOMultiplier(4);

    //    SnapshotManagement::setNumberOfSCOSBetweenCheck(scosInTlog);
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns,
                          VolumeSize((1 << 18) * 512),
                          clustersInSco);

    const int count = v->getClusterSize();

    const std::string snapshotName1("nikon1");

    writeToVolume(v, 0, count, snapshotName1);

    createSnapshot(v, snapshotName1);
    waitForThisBackendWrite(v);

    //make sure we delete some tlog in the middle, not just the first one
    for (uint32_t i = 0 ; i < 5 * clustersInSco * scosInTlog; i++) {
        writeToVolume(v, 0, count, "there are no rules without exceptions, so there are rules without exceptions");
    }

    const std::string tlogname = v->getSnapshotManagement().getCurrentTLogName();

    for (uint32_t i = 0 ; i < 5 * clustersInSco * scosInTlog; i++) {
        writeToVolume(v, 0, count, "there are no rules without exceptions, so there are rules without exceptions");
    }

    const std::string snapshotName2("nikon2");

    createSnapshot(v, snapshotName2);
    waitForThisBackendWrite(v);

    v->getBackendInterface()->remove(tlogname);


    ASSERT_NO_THROW(restoreSnapshot(v, snapshotName1));
    checkVolume(v,0,count,snapshotName1);
}

TEST_P(SimpleVolumeTest, restoreSnapshotWithFuckedUpNeededTLogs)
{
    uint32_t scosInTlog    = 1;
    SCOMultiplier clustersInSco = SCOMultiplier(4);

    //SnapshotManagement::setNumberOfSCOSBetweenCheck(scosInTlog);
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns,
                          VolumeSize((1 << 18) * 512),
                          clustersInSco);

    const int count = v->getClusterSize();

    const std::string snapshotName1("nikon1");

    //make sure we delete some tlog in the middle, not just the first one
    for (uint32_t i = 0 ; i < 5 * clustersInSco * scosInTlog; i++) {
        writeToVolume(v, 0, count, "should be overwritten");
    }

    const std::string tlogname = v->getSnapshotManagement().getCurrentTLogName();

    for (uint32_t i = 0 ; i < 5 * clustersInSco * scosInTlog; i++) {
        writeToVolume(v, 0, count, "should be overwritten");
    }

    writeToVolume(v, 0, count, snapshotName1);

    for (uint32_t i = 0 ; i < 5 * clustersInSco * scosInTlog; i++) {
        writeToVolume(v, 0, count, "there are no rules without exceptions, so there are rules without exceptions");
    }

    createSnapshot(v, snapshotName1);
    waitForThisBackendWrite(v);

    const std::string snapshotName2("nikon2");

    createSnapshot(v, snapshotName2);
    waitForThisBackendWrite(v);

    v->getBackendInterface()->remove(tlogname);


    ASSERT_THROW(restoreSnapshot(v, snapshotName1), std::exception);
}


TEST_P(SimpleVolumeTest, dontLeakStorageOnSnapshotRollback)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    //const Namespace nspace;
    Volume* v = newVolume(VolumeId("volume1"),
                          nspace);

    const int size = v->getClusterSize();

    const std::string pattern1 = "sco1";
    writeToVolume(v, 0, size, pattern1);
    createSnapshot(v, "snap1");

    waitForThisBackendWrite(v);

    const std::string pattern2 = "sco2";
    writeToVolume(v, 0, size, pattern2);
    createSnapshot(v, "snap2");

    waitForThisBackendWrite(v);

    const std::string scoGlob("00_.*");
    const std::string tlogGlob("tlog_.*");

    std::list<std::string> scos;
    std::list<std::string> tlogs;

    backendRegex(nspace,
                 scoGlob,
                 scos);

    backendRegex(nspace,
                 tlogGlob,
                 tlogs);

    EXPECT_EQ(2U, scos.size());
    EXPECT_EQ(2U, tlogs.size());


    ASSERT_NO_THROW(restoreSnapshot(v, "snap1"));

    scos.clear();
    tlogs.clear();
    waitForThisBackendWrite(v);

    backendRegex(nspace,
                 scoGlob,
                 scos);

    backendRegex(nspace,
                 tlogGlob,
                 tlogs);

    EXPECT_EQ(1U, scos.size()) << "sco leaked";
    EXPECT_EQ(1U, tlogs.size()) << "tlog leaked";
}

TEST_P(SimpleVolumeTest, cleanupAfterInvalidVolume)
{
    Volume* v = 0;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    ASSERT_THROW(v = newVolume(VolumeId("volume1"),
			       ns,
			       VolumeSize(13),
			       SCOMultiplier(1)),
                 fungi::IOException);

    ASSERT_FALSE(v);
    ASSERT_NO_THROW(v = newVolume(VolumeId("volume1"),
				  ns));

    EXPECT_TRUE(v);

    ASSERT_NO_THROW(destroyVolume(v,
                                  DeleteLocalData::T,
                                  RemoveVolumeCompletely::T));
}


TEST_P(SimpleVolumeTest, reCreateVol)
{
    Volume* v1 = nullptr;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    ASSERT_NO_THROW(v1 = newVolume(VolumeId("volume1"),
				   ns));
    ASSERT_TRUE(v1);

    ASSERT_THROW(newVolume(VolumeId("volume1"),
                           ns),
                 fungi::IOException);

    ASSERT_NO_THROW(destroyVolume(v1,
                                  DeleteLocalData::T,
                                  RemoveVolumeCompletely::T));
}

TEST_P(SimpleVolumeTest, createUseAndDestroyVolume)
{
    const VolumeId volname("volname");

    Volume* v = 0;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    ASSERT_NO_THROW(v = newVolume(volname,
				  ns));
    ASSERT_TRUE(v);

    const SnapshotName snap("snap1");
    ASSERT_NO_THROW(v->createSnapshot(snap));

    while(not isVolumeSyncedToBackend(v))
    {
        sleep(1);
    }


    //    ASSERT_THROW(v->restoreSnapshot("snap1"),fungi::IOException);

    ASSERT_NO_THROW(v->restoreSnapshot(snap));
    ASSERT_NO_THROW(destroyVolume(v,
                                  DeleteLocalData::T,
                                  RemoveVolumeCompletely::T));
}


TEST_P(SimpleVolumeTest, RollBack)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    const SnapshotName snap("snap1");
    Volume* v1 = newVolume("1volume",
                           ns);
    {
        SCOPED_BLOCK_BACKEND(v1);
        v1->createSnapshot(snap);

        EXPECT_THROW(v1->restoreSnapshot(snap),
                     fungi::IOException);



    }
    waitForThisBackendWrite(v1);
    waitForThisBackendWrite(v1);

    EXPECT_NO_THROW(v1->restoreSnapshot(snap));

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(SimpleVolumeTest, RollBack2)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = newVolume("1volume",
			   ns);

    const SnapshotName snap("snap1");
    v1->createSnapshot(snap);
    waitForThisBackendWrite(v1);

    {
        SCOPED_BLOCK_BACKEND(v1);
        v1->createSnapshot(SnapshotName("snap2"));
    }

    v1->restoreSnapshot(snap);

    waitForThisBackendWrite(v1);
    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(SimpleVolumeTest, LocalRestartClone)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    //    backend::Namespace ns1;

    Volume* v1 = newVolume("1volume",
			   ns1);
    for(int i = 0; i < 10; i++)
    {
        writeToVolume(v1,
                      0,
                      4096,
                      "immanuel");
    }

    v1->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(v1);
    waitForThisBackendWrite(v1);

    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& clone_ns = ns1_ptr->ns();

    // backend::Namespace clone_ns;

    Volume* c = createClone("clone1",
                            clone_ns,
                            ns1,
                            "snap1");
    destroyVolume(c,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    ASSERT_NO_THROW(localRestart(clone_ns));

    c = 0;
    c = getVolume(VolumeId("clone1"));
    ASSERT_TRUE(c);

    checkVolume(c,
                0,
                4096,
                "immanuel");
}

TEST_P(SimpleVolumeTest, RollBackClone)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    // const backend::Namespace ns1;

    Volume* v1 = newVolume("1volume",
			   ns1);
    const int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 50; i++)
    {

        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "immanuel");
    }

    const SnapshotName snap1("snap1");

    v1->createSnapshot(snap1);
    waitForThisBackendWrite(v1);
    waitForThisBackendWrite(v1);

    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    // const backend::Namespace ns2;

    Volume* v2 = createClone("2volume",
                             ns2,
                             ns1,
                             snap1);

    auto ns3_ptr = make_random_namespace();

    const backend::Namespace& ns3 = ns3_ptr->ns();

    // const backend::Namespace ns3;
    Volume* v3 = newVolume("3volume",
                           ns3);

    for(int i = 50; i < 100; i++)
    {
        writeToVolume(v2,
                      i*mult,
                      v1->getClusterSize(),
                      "arne");
        writeToVolume(v3,
                      i*mult,
                      v1->getClusterSize(),
                      "arne");
        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "arne");
    }

    EXPECT_FALSE(checkVolumesIdentical(v2,v3));
    EXPECT_TRUE(checkVolumesIdentical(v1,v2));
    v2->createSnapshot(snap1);

    for(int i = 50; i < 100; i++)
    {
        writeToVolume(v2,
                      i*mult,
                      v1->getClusterSize(),
                      "kristaf");
    }

    EXPECT_FALSE(checkVolumesIdentical(v3,v2));
    EXPECT_FALSE(checkVolumesIdentical(v1,v2));
    waitForThisBackendWrite(v2);
    waitForThisBackendWrite(v2);

    v2->restoreSnapshot(snap1);

    EXPECT_FALSE(checkVolumesIdentical(v2,v3));
    EXPECT_TRUE(checkVolumesIdentical(v2,v1));
}

namespace
{
double
timeval_subtract (struct timeval& x, struct timeval&  y)
{
     /* Perform the carry for the later subtraction by updating Y. */
     if (x.tv_usec < y.tv_usec) {
         long int nsec = (y.tv_usec - x.tv_usec) / 1000000 + 1;
         y.tv_usec -= 1000000 * nsec;
         y.tv_sec += nsec;
     }
     if (x.tv_usec - y.tv_usec > 1000000) {
         long int nsec = (x.tv_usec - y.tv_usec) / 1000000;
         y.tv_usec += 1000000 * nsec;
         y.tv_sec -= nsec;
     }

     /* Compute the time remaining to wait.
        `tv_usec' is certainly positive. */
     return static_cast<double>(x.tv_sec - y.tv_sec) +
         (static_cast<double>((double)x.tv_usec - (double)y.tv_usec)
          /
          (double)1000000.0);
 }
template<typename T>
void
print_vector(const std::vector<T>& vec)
{
    size_t sz = vec.size();

    for(size_t i = 0; i < sz - 1; i ++)
    {
        std::cout << vec[i] << ", " ;
    }
    std::cout << vec[sz-1];
}

unsigned
get_vmpeak()
{
    const std::string filename("/proc/self/status");
    std::ifstream file(filename.c_str());
    std::string line;

    do
    {
        getline(file,line);
        if(line.find("VmPeak:") == 0)
        {
            size_t start = line.find_first_of("0123456789");
            assert(start != std::string::npos);
            size_t end = line.find_last_of("0123456789");
            assert(end != std::string::npos);
            assert(end > start);

            std::stringstream ss(line.substr(start, end - start));
            unsigned int ret = 0;
            ss >> ret;
            return ret;
        }
    }   while(not file.eof());

    assert(0 == "We should not get here");
    return 0; // Added to fix compilation errors
}

}

TEST_P(SimpleVolumeTest, DISABLED_Memory)
{
    std::cout << "VmPeak: " << get_vmpeak() <<std::endl;
    for(int i =0; i < 20; i++)
    {
        TODO("Y42 NON RANDOMIZE NAMESPACE");
        std::stringstream ss;
        ss << "vol" << i;
        newVolume(ss.str(),
		  backend::Namespace("openvstorage-volumedrivertest-namespace" + ss.str()));
        std::cout << "VmPeak: " << get_vmpeak() << std::endl;
    }
}

TEST_P(SimpleVolumeTest, DISABLED_SnapPerformance)
{
    struct timeval start, end;

    std::vector<unsigned> numbers;
    numbers.reserve(100);

    std::vector<long int> max_resident;
    max_resident.reserve(100);

    std::vector<double> snapshottimes, restarttimes;

    snapshottimes.reserve(100);

    Volume* vol = newVolume("vol1",
			    backend::Namespace());
    VolumeConfig cfg = vol->get_config();
    //struct rusage rus;
    for(int i = 0; i < 20; ++i)
    {
        Volume* v = getVolume(VolumeId("vol1"));
        ASSERT_TRUE(v);
        LOG_INFO("Run " << i);
        size_t upto = 50;

        for(size_t j = 0; j < upto -1; ++j)
        {
            std::stringstream ss;
            ss << (i*upto) + j;
            createSnapshot(v,ss.str());
        }
        LOG_INFO("Creating ze bigsnap");

        std::stringstream ss;
        ss << "BigSnap_" << i;

        gettimeofday(&start, 0);
        createSnapshot(v,ss.str());
        gettimeofday(&end,0);

        numbers.push_back(i*upto);

        snapshottimes.push_back(timeval_subtract(end,start));


        max_resident.push_back(get_vmpeak());

        v->scheduleBackendSync();
        while(not v->isSyncedToBackend())
        {
            sleep(1);
        }

        destroyVolume(v,
                      DeleteLocalData::T,
                      RemoveVolumeCompletely::F);

        gettimeofday(&start,0);
        restartVolume(cfg);
        gettimeofday(&end,0);
        restarttimes.push_back(timeval_subtract(end,start));
    }
    std::cout << "number: [";
    print_vector(numbers);
    std::cout << "]" << std::endl;


    std::cout << "snapshottimes: [";
    print_vector(snapshottimes);
    std::cout << "]" << std::endl;

    std::cout << "rusage.ru_maxrss: [";
    print_vector(max_resident);
    std::cout << "]" << std::endl;

    std::cout << "restarttimes: [";
    print_vector(restarttimes);
    std::cout << "]" << std::endl;

}

TEST_P(SimpleVolumeTest, backend_sync_up_to_snapshot)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = newVolume(VolumeId("volume1"),
			   ns);

    const SnapshotName snap("snap1");

    EXPECT_THROW(v1->isSyncedToBackendUpTo(snap),
                 fungi::IOException);

    {
        SCOPED_BLOCK_BACKEND(v1);

            v1->createSnapshot(snap);
            EXPECT_FALSE(v1->isSyncedToBackendUpTo(snap));
    }

    waitForThisBackendWrite(v1);
    EXPECT_TRUE(v1->isSyncedToBackendUpTo(snap));
}

TEST_P(SimpleVolumeTest, backend_sync_up_to_tlog)
{
    auto ns(make_random_namespace());
    Volume* v = newVolume(*ns);

    const SnapshotManagement& sm = v->getSnapshotManagement();
    const TLogID old_tlog_id(TLog::getTLogIDFromName(sm.getCurrentTLogName()));

    EXPECT_FALSE(v->isSyncedToBackendUpTo(old_tlog_id));

    TLogID new_tlog_id;

    {
        SCOPED_BLOCK_BACKEND(v);

        EXPECT_EQ(old_tlog_id,
                  v->scheduleBackendSync());

        new_tlog_id = TLog::getTLogIDFromName(sm.getCurrentTLogName());

        EXPECT_NE(old_tlog_id,
                  new_tlog_id);

        EXPECT_FALSE(v->isSyncedToBackendUpTo(old_tlog_id));
        EXPECT_FALSE(v->isSyncedToBackendUpTo(new_tlog_id));
    }

    waitForThisBackendWrite(v);

    EXPECT_TRUE(v->isSyncedToBackendUpTo(old_tlog_id));
    EXPECT_FALSE(v->isSyncedToBackendUpTo(new_tlog_id));
}

TEST_P(SimpleVolumeTest, consistency)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = newVolume(VolumeId("volume1"),
			   ns);
    const int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 1024; i++)
    {

        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "kristaf");
    }

    EXPECT_THROW(v1->checkConsistency(), fungi::IOException);


    v1->scheduleBackendSync();
    while(not v1->isSyncedToBackend())
    {
        usleep(1000);
    }

    ASSERT_TRUE(v1->checkConsistency());
}

TEST_P(SimpleVolumeTest, consistencyClone)
{
    // const backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume(VolumeId("volume1"),
			   ns1);
    const int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < (1024 ); i++)
    {

        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "kristaf");
    }

    v1->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(v1);

    // backend::Namespace ns2;
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    Volume* v2 = createClone("volume2",
                             ns2,
                             ns1,
                             "snap1");
    for(int i = 0; i < (1024 ); i++)
    {
        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "kristaf");

        writeToVolume(v2,
                      i*mult + 20,
                      v2->getClusterSize(),
                      "kristaf");
    }
    // these are throwing on the fact that there is a tlog not written to backend
    // but with small tlogsizes that doesn't need to be the case
    // EXPECT_THROW(v1->checkConsistency(), fungi::IOException);
    // EXPECT_THROW(v2->checkConsistency(), fungi::IOException);

    v1->scheduleBackendSync();
    v2->scheduleBackendSync();
    while(not v1->isSyncedToBackend())
    {
        usleep(1000);
    }
    while(not v2->isSyncedToBackend())
    {
        usleep(1000);
    }


    ASSERT_TRUE(v1->checkConsistency());

    ASSERT_TRUE(v2->checkConsistency());
}

TEST_P(SimpleVolumeTest,  readCacheHits)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume(VolumeId("volume1"),
                           ns1);
    writeToVolume(v1,
                  0,
                  v1->getClusterSize(),
                  "kristafke");

    fungi::ScopedLock l(api::getManagementMutex());
    uint64_t  hits =  api::getClusterCacheHits(VolumeId("volume1"));
    uint64_t  misses =  api::getClusterCacheMisses(VolumeId("volume1"));
    EXPECT_EQ(0U, hits);
    EXPECT_EQ(0U, misses);
    checkVolume(v1,0, v1->getClusterSize(), "kristafke");
    hits =  api::getClusterCacheHits(VolumeId("volume1"));
    misses =  api::getClusterCacheMisses(VolumeId("volume1"));
    EXPECT_EQ(0U, hits);
    EXPECT_EQ(1U, misses);

    checkVolume(v1,0, v1->getClusterSize(), "kristafke");
    hits =  api::getClusterCacheHits(VolumeId("volume1"));
    misses =  api::getClusterCacheMisses(VolumeId("volume1"));
    EXPECT_EQ(1U, hits);
    EXPECT_EQ(1U, misses);

    checkVolume(v1,0, v1->getClusterSize(), "kristafke");
    hits =  api::getClusterCacheHits(VolumeId("volume1"));
    misses =  api::getClusterCacheMisses(VolumeId("volume1"));
    EXPECT_EQ(2U, hits);
    EXPECT_EQ(1U, misses);

    checkVolume(v1,0, v1->getClusterSize(), "kristafke");
    hits =  api::getClusterCacheHits(VolumeId("volume1"));
    misses =  api::getClusterCacheMisses(VolumeId("volume1"));
    EXPECT_EQ(3U, hits);
    EXPECT_EQ(1U, misses);
}


TEST_P(SimpleVolumeTest, backendSize)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume(VolumeId("volume1"),
			   ns1);
    const unsigned int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 1024 ; i++)
    {

        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "kristaf");
    }
    waitForThisBackendWrite(v1);

    uint64_t size = 0;
    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 1024 * v1->getClusterSize());

    const SnapshotName snap("testsnap");

    {
        SCOPED_BLOCK_BACKEND(v1);
        v1->createSnapshot(snap);
        size = 0;
        ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
        EXPECT_TRUE(size == 0);
        size = 0;
        ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap));
        EXPECT_TRUE(size == 1024 * v1->getClusterSize());

    }
    waitForThisBackendWrite(v1);

    ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap));

    EXPECT_TRUE(size == 1024 * v1->getClusterSize());
    for(int i = 0; i < 1024 ; i++)
    {
        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "kristaf");
    }
    waitForThisBackendWrite(v1);
    size =  0;
    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 1024 * v1->getClusterSize());
}

TEST_P(SimpleVolumeTest, backendSize2)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume(VolumeId("volume1"),
                           ns1);
    uint64_t size =  0;
    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 0);

    const SnapshotName snap("testsnap");
    {
        SCOPED_BLOCK_BACKEND(v1);

        v1->createSnapshot(snap);
        size = 0;
        ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap));
        EXPECT_TRUE(size == 0);
    }


    waitForThisBackendWrite(v1);
    size = 0;
    ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap));
    EXPECT_TRUE(size == 0);
    const unsigned int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 1024 ; i++)
    {
        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "kristaf");
    }
    waitForThisBackendWrite(v1);

    size =  0;
    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 1024 * v1->getClusterSize());
}

TEST_P(SimpleVolumeTest, backendSize3)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume(VolumeId("volume1"),
			   ns1);
    const unsigned int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 1024 ; i++)
    {

        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "kristaf");
    }

    const SnapshotName snap("testsnap");

    v1->createSnapshot(snap);
    waitForThisBackendWrite(v1);
    uint64_t size = 0;
    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 0);
    ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap));
    EXPECT_TRUE(size  == 1024 * v1->getClusterSize());
    v1->deleteSnapshot(snap);

    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 1024 * v1->getClusterSize());
    EXPECT_THROW(size = v1->getSnapshotBackendSize(snap), fungi::IOException);
    //    EXPECT_TRUE(size  == 1024 * v1->getClusterSize()0);
    v1->createSnapshot(snap);
    for(int i = 0; i < 1024 ; i++)
    {

        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "joost");
    }
    waitForThisBackendWrite(v1);

    const SnapshotName snap2("testsnap2");
    v1->createSnapshot(snap2);
    waitForThisBackendWrite(v1);

    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 0);
    ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap));
    EXPECT_TRUE(size  == 1024 * v1->getClusterSize());
    ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap2));
    EXPECT_TRUE(size  == 1024 * v1->getClusterSize());
    v1->deleteSnapshot(snap);
    ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap2));
    EXPECT_TRUE(size  == 2048 * v1->getClusterSize());
    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 0);
}

TEST_P(SimpleVolumeTest, writeConfigToBackend)
{
    // backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume("v1",
			   ns1);

    std::unique_ptr<VolumeConfig> f;
    ASSERT_NO_THROW(f = VolManager::get()->get_config_from_backend<VolumeConfig>(ns1));
    ASSERT_NO_THROW(writeVolumeConfigToBackend(v1));
    ASSERT_NO_THROW(f = VolManager::get()->get_config_from_backend<VolumeConfig>(ns1));
    const VolumeConfig cf1 = v1->get_config();
    EXPECT_TRUE(cf1.id_ == f->id_);
    EXPECT_TRUE(cf1.getNS() == f->getNS());
    EXPECT_TRUE(cf1.parent_ns_ ==  f->parent_ns_);
    EXPECT_EQ(cf1.parent_snapshot_, f->parent_snapshot_);
    EXPECT_TRUE(cf1.lba_size_ == f->lba_size_);
    EXPECT_EQ(cf1.lba_count(), f->lba_count());
    EXPECT_TRUE(cf1.cluster_mult_ == f->cluster_mult_);
    EXPECT_TRUE(cf1.sco_mult_ == f->sco_mult_);
}

TEST_P(SimpleVolumeTest, readActivity)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume("v1",
			   ns1);
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    Volume* v2 = newVolume("v2",
			   ns2);
    ASSERT_EQ(v1->readActivity(),0);
    ASSERT_EQ(v2->readActivity(),0);
    ASSERT_EQ(VolManager::get()->readActivity(),0);


    sleep(1);
    {
        fungi::ScopedLock l(VolManager::get()->getLock_());
        VolManager::get()->updateReadActivity();
    }

    ASSERT_EQ(v1->readActivity(),0);
    ASSERT_EQ(v2->readActivity(),0);
    ASSERT_EQ(VolManager::get()->readActivity(),0);

    uint8_t buf[8192];
    for(int i = 0; i < 128; ++i)
    {
        v1->read(0,buf,4092);
        v2->read(0,buf, 8192);
    }
    sleep(1);

    {
        fungi::ScopedLock l(VolManager::get()->getLock_());
        VolManager::get()->updateReadActivity();
    }
    double ra1 = v1->readActivity();
    double ra2 = v2->readActivity();
    double rac = VolManager::get()->readActivity();

    ASSERT_TRUE(ra1 > 0);
    ASSERT_TRUE(ra2 > 0);
    ASSERT_EQ(2* ra1, ra2);
    ASSERT_EQ(rac, ra1+ra2);

}

TEST_P(SimpleVolumeTest, prefetch)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    // const backend::Namespace nspace;
    const VolumeId volname("volume");

    Volume* v = newVolume(volname,
			  nspace);

    ASSERT_TRUE(v);

    VolManager* vm = VolManager::get();

    ASSERT_NO_THROW(updateReadActivity());
    persistXVals(volname);

    VolumeConfig cfg = v->get_config();

    const unsigned num_scos = 7;
    const uint64_t size = cfg.lba_size_ * cfg.cluster_mult_ * cfg.sco_mult_ * num_scos;
    const std::string pattern("prefetchin'");
    writeToVolume(v,
                  0,
                  size,
                  pattern);

    std::vector<uint8_t> buf(size);


    for(int i =0 ; i< 64; ++i)
    {

        v->read(0, &buf[0], size);
    }

    v->scheduleBackendSync();
    while(!v->isSyncedToBackend())
    {
        sleep(1);
    }


    SCOCache* c = VolManager::get()->getSCOCache();

    SCONameList l;

    EXPECT_TRUE(c->hasNamespace(nspace));

    c->getSCONameList(nspace,
                      l,
                      true);

    EXPECT_EQ(num_scos, l.size());
    sleep(1);

    ASSERT_NO_THROW(updateReadActivity());
    persistXVals(volname);
    EXPECT_LT(0U, vm->readActivity());
    EXPECT_EQ(0U, v->getCacheMisses());
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    EXPECT_FALSE(c->hasNamespace(nspace));

    restartVolume(cfg,
                  PrefetchVolumeData::T);

    v = getVolume(volname);
    ASSERT_TRUE(v);
    sleep(1);

    waitForPrefetching(v);

    EXPECT_TRUE(c->hasNamespace(nspace));

    l.clear();
    c->getSCONameList(nspace,
                      l,
                      true);

    EXPECT_EQ(num_scos, l.size());
    EXPECT_LT(0U, vm->readActivity());

    checkVolume(v,
                0,
                size,
                pattern);

    ASSERT_NO_THROW(updateReadActivity());

    EXPECT_LT(0U, vm->readActivity());
    EXPECT_EQ(0U, v->getCacheMisses());
}

TEST_P(SimpleVolumeTest, startPrefetch)
{
    VolumeId vid("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v = newVolume(vid,
                          ns1);
    const std::string pattern("prefetchin'");
    EXPECT_NO_THROW(v->startPrefetch());
    VolumeConfig cfg = v->get_config();

    const int num_scos = 7;
    const uint64_t size = cfg.lba_size_ * cfg.cluster_mult_ * cfg.sco_mult_ * num_scos;

    writeToVolume(v,
                  0,
                  size,
                  pattern);

    ASSERT_NO_THROW(updateReadActivity());
    persistXVals(vid);
    waitForThisBackendWrite(v);

    ASSERT_NO_THROW(v->startPrefetch());
    sleep(1);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

// test whether SCOs span tlog boundaries
// it relies on the following behaviour of the current implementation:
// -- volume: cluster writes are only clustered if they don't exceed the sco size
// -- snapshotmanagement: tlogs are rolled over after > maxTimeBetweenTLogs if
//    numberOfSCOSBetweenCheck * scoMultiplier entries were written
TEST_P(SimpleVolumeTest, clusteredWriteTLogConsistency)
{
    //SnapshotManagement::setNumberOfSCOSBetweenCheck(1);
    // BOOST_SCOPE_EXIT()
    // {
    //     //SnapshotManagement::setNumberOfSCOSBetweenCheck(20);
    // } BOOST_SCOPE_EXIT_END;


    const std::string name("volume");
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(name, ns);

    ASSERT_TRUE(v);

    writeToVolume(v,
                  0,
                  v->getClusterSize(),
                  "abcd");

    // required to make the TLog roll over
    sleep(1);

    writeToVolume(v,
                  v->getClusterSize() / v->getLBASize(),
                  v->getClusterSize() * v->getSCOMultiplier(),
                  "defg");


    v->scheduleBackendSync();
    waitForThisBackendWrite(v);

    EXPECT_TRUE(v->checkConsistency());
}
#if 0
TEST_P(SimpleVolumeTest, replicationID)
{
    newVolume(VolumeId("a"),
              backend::Namespace());
    Volume* b = newVolume(VolumeId("b"),
                          backend::Namespace());
    newVolume(VolumeId("c"),
              backend::Namespace());
    destroyVolume(b,
                  DeleteLocalData::T,
                  true);
    newVolume(VolumeId("d"),
              backend::Namespace());

    {
        fungi::ScopedLock m(VolManager::get()->getLock_());
        EXPECT_EQ(ReplicationID(0), VolManager::get()->getReplicationID(VolumeId("a")));
        EXPECT_EQ(ReplicationID(2), VolManager::get()->getReplicationID(VolumeId("c")));
        EXPECT_EQ(ReplicationID(1), VolManager::get()->getReplicationID(VolumeId("d")));
    }
}
#endif
TEST_P(SimpleVolumeTest, zero_sized_volume)
{
    const std::string vname("zero");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    //    const backend::Namespace ns;

    Volume* v = newVolume(vname, ns, VolumeSize(0));
    EXPECT_TRUE(v != nullptr);

    EXPECT_EQ(0U, v->getLBACount());
    EXPECT_EQ(0U, v->getSize());

    uint8_t pattern = 0xab;
    const std::vector<uint8_t> wbuf(v->getClusterSize(), pattern);
    EXPECT_THROW(v->write(0, &wbuf[0], wbuf.size()),
                 std::exception);

    ++pattern;
    std::vector<uint8_t> rbuf(v->getClusterSize(), pattern);
    EXPECT_THROW(v->read(0, &rbuf[0], rbuf.size()),
                 std::exception);

    for (const auto& b : rbuf)
    {
        EXPECT_EQ(pattern, b);
    }
}

TEST_P(SimpleVolumeTest, shrink)
{
    const std::string vname("volume");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    // const backend::Namespace ns;
    Volume* v = newVolume(vname,
                          ns);

    EXPECT_TRUE(v != nullptr);
    const uint64_t vsize = v->getSize();
    const uint64_t csize = v->getClusterSize();

    const uint64_t nclusters = (vsize - csize) / csize;
    EXPECT_LT(0U, nclusters);

    EXPECT_THROW(v->resize(nclusters), std::exception);

    EXPECT_EQ(vsize, v->getSize());
}

TEST_P(SimpleVolumeTest, grow)
{
    const std::string vname("volume");
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(vname,
                          ns);

    EXPECT_TRUE(v != nullptr);
    const uint64_t vsize = v->getSize();
    const uint64_t csize = v->getClusterSize();

    const uint64_t nclusters = (vsize + csize) / csize;
    EXPECT_LT(vsize, nclusters * csize);

    v->resize(nclusters);

    EXPECT_EQ(nclusters * csize, v->getSize());

    const std::string pattern("extended play");
    uint64_t lba((nclusters - 1) * csize / v->getLBASize());

    writeToVolume(v, lba, csize, pattern);
    checkVolume(v, lba, csize, pattern);
}

TEST_P(SimpleVolumeTest, grow_beyond_end)
{
    const std::string vname("volume");
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(vname,
                          ns);

    EXPECT_TRUE(v != nullptr);

    const uint64_t vsize = v->getSize();
    const uint64_t csize = v->getClusterSize();
    const uint64_t nclusters = 1 + VolManager::get()->real_max_volume_size() / csize;

    EXPECT_THROW(v->resize(nclusters), std::exception);

    EXPECT_EQ(vsize, v->getSize());
}

TEST_P(SimpleVolumeTest, tlog_entries)
{
    const std::string vname("volume");
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(vname,
                          ns);

    const uint64_t csize = v->getClusterSize();
    const SCOMultiplier sco_mult = v->getSCOMultiplier();
    auto& sm = v->getSnapshotManagement();
    const unsigned tlog_entries = sm.maxTLogEntries();
    const auto tlog1(sm.getCurrentTLogPath());

    EXPECT_LE(sco_mult, tlog_entries);
    EXPECT_EQ(0U, tlog_entries % sco_mult);

    // block the backend to spare us the exercise of having
    // to fetch the TLog
    SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, 2,
                                          DeleteLocalData::T,
                                          RemoveVolumeCompletely::F);

    for (unsigned i = 0; i < sco_mult; ++i)
    {
        writeToVolume(v, 0, csize, "not of any importance");
    }

    // should not have any impact.
    v->sync();

    EXPECT_EQ(tlog1, sm.getCurrentTLogPath());

    for (unsigned i = sco_mult; i < tlog_entries; ++i)
    {
        EXPECT_EQ(tlog1, sm.getCurrentTLogPath());
        writeToVolume(v, 0, csize, "neither of importance");
    }

    // tlog should've rolled over by now:
    const auto tlog2(sm.getCurrentTLogPath());
    EXPECT_TRUE(tlog1 != tlog2);

    TLogReader tr(tlog1);
    for (unsigned i = 0; i < tlog_entries / sco_mult; ++i)
    {
        for (unsigned j = 0; j < sco_mult; ++j)
        {
            auto e = tr.nextAny();
            ASSERT_TRUE(e != nullptr);
            EXPECT_TRUE(e->isLocation());
        }

        auto e = tr.nextAny();
        ASSERT_TRUE(e != nullptr);
        EXPECT_TRUE(e->isSCOCRC());
    }

    auto e = tr.nextAny();
    ASSERT_TRUE(e != nullptr);
    EXPECT_TRUE(e->isTLogCRC());

    EXPECT_EQ(nullptr, tr.nextAny());
}

TEST_P(SimpleVolumeTest, sco_cache_limits)
{
    //    const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns);

    auto vm = VolManager::get();

    const PARAMETER_TYPE(non_disposable_scos_factor)::ValueType factor =
        vm->non_disposable_scos_factor.value();
    ASSERT_LE(1, factor);

    const PARAMETER_TYPE(number_of_scos_in_tlog)::ValueType scos =
        vm->number_of_scos_in_tlog.value();

    const uint64_t scosize = v->getSCOSize();
    const uint64_t max_non_disposable = scosize * scos * factor;
    const SCOCacheNamespaceInfo info(vm->getSCOCache()->getNamespaceInfo(ns));

    EXPECT_EQ(0U, info.min);
    EXPECT_EQ(max_non_disposable, info.max_non_disposable);
}

// exposes / verifies fix for OVS-1309
TEST_P(SimpleVolumeTest, simple_backend_restart)
{
    const VolumeId id("volume");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();
    // const be::Namespace ns(id);

    Volume* v = newVolume(id,
                          ns);

    const VolumeConfig cfg = v->get_config();

    const std::string pattern("Hullo?");
    writeToVolume(v,
                  0,
                  4096,
                  pattern);

    v->scheduleBackendSync();
    while(not v->isSyncedToBackend())
    {
        boost::this_thread::sleep_for(boost::chrono::seconds(1));
    }

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    restartVolume(cfg);

    v = getVolume(id);

    checkVolume(v,
                0,
                4096,
                pattern);
}

// We don't have a test for that yet?
TEST_P(SimpleVolumeTest, no_snapshot_if_previous_snapshot_is_not_yet_in_the_backend)
{
    with_snapshot_not_in_backend([this](Volume& v,
                                        const std::string& /* snap */)
                                 {
                                     ASSERT_THROW(createSnapshot(&v,
                                                                 "no_snapshot"),
                                                  std::exception);
                                 });
}

TEST_P(SimpleVolumeTest, no_clone_if_snapshot_is_not_yet_in_the_backend)
{
    with_snapshot_not_in_backend([this](Volume& v,
                                        const std::string& snap)
                                 {
                                     auto ns(make_random_namespace());
                                     ASSERT_THROW(createClone(*ns,
                                                              v.getNamespace(),
                                                              snap),
                                                  std::exception);
                                 });
}

// OVS-2659: an assertion in ClusterCache::add is triggered when called from a clone
// caused by the clone not being registered with the ClusterCache.
TEST_P(SimpleVolumeTest, clones_and_location_based_caching)
{
    const ClusterCacheMode ccmode = ClusterCacheMode::LocationBased;
    set_cluster_cache_default_mode(ccmode);

    const ClusterCacheBehaviour ccbehaviour = ClusterCacheBehaviour::CacheOnRead;
    set_cluster_cache_default_behaviour(ccbehaviour);

    const auto parent_ns(make_random_namespace());
    Volume* parent = newVolume(*parent_ns);

    ASSERT_EQ(ccmode,
              parent->effective_cluster_cache_mode());

    ASSERT_EQ(ccbehaviour,
              parent->effective_cluster_cache_behaviour());

    const std::string pattern("written-to-parent");
    writeToVolume(parent,
                  0,
                  4096,
                  pattern);

    const std::string snap("snap");
    createSnapshot(parent,
                   snap);

    waitForThisBackendWrite(parent);
    ASSERT_TRUE(parent->getSnapshotManagement().lastSnapshotOnBackend());

    const auto clone_ns(make_random_namespace());
    Volume* clone = createClone(*clone_ns,
                                parent_ns->ns(),
                                snap);

    ASSERT_EQ(ccmode,
              clone->effective_cluster_cache_mode());

    ASSERT_EQ(ccbehaviour,
              clone->effective_cluster_cache_behaviour());

    checkVolume(clone,
                0,
                4096,
                pattern);
}

TEST_P(SimpleVolumeTest, cluster_cache_limit)
{
    auto ns(make_random_namespace());
    Volume* v = newVolume(*ns);

    EXPECT_EQ(boost::none,
              v->get_cluster_cache_limit());

    EXPECT_EQ(boost::none,
              v->get_config().cluster_cache_limit_);

    EXPECT_THROW(v->set_cluster_cache_limit(ClusterCount(0)),
                 std::exception);

    EXPECT_EQ(ClusterCacheMode::ContentBased,
              v->effective_cluster_cache_mode());

    const ClusterCount max(7);

    v->set_cluster_cache_limit(max);

    EXPECT_EQ(max,
              *v->get_cluster_cache_limit());

    EXPECT_EQ(max,
              *v->get_config().cluster_cache_limit_);

    ClusterCache& cc = VolManager::get()->getClusterCache();

    EXPECT_EQ(boost::none,
              cc.get_max_entries(v->getClusterCacheHandle()));

    v->set_cluster_cache_mode(ClusterCacheMode::LocationBased);

    EXPECT_EQ(max,
              *v->get_cluster_cache_limit());
    EXPECT_EQ(max,
              ClusterCount(*cc.get_max_entries(v->getClusterCacheHandle())));

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    localRestart(ns->ns());
    v = getVolume(VolumeId(ns->ns().str()));

    EXPECT_EQ(ClusterCacheMode::LocationBased,
              v->effective_cluster_cache_mode());

    EXPECT_EQ(max,
              *v->get_config().cluster_cache_limit_);

    EXPECT_EQ(max,
              *v->get_cluster_cache_limit());

    EXPECT_EQ(max,
              ClusterCount(*cc.get_max_entries(v->getClusterCacheHandle())));

    v->set_cluster_cache_limit(boost::none);

    EXPECT_EQ(boost::none,
              v->get_cluster_cache_limit());
    EXPECT_EQ(boost::none,
              cc.get_max_entries(v->getClusterCacheHandle()));
}

TEST_P(SimpleVolumeTest, cluster_cache_handle)
{
    auto ns(make_random_namespace());
    Volume* v = newVolume(*ns);

    // TODO: hmm, this is a ClusterCache internal convention that probably
    // shouldn't be used here.
    const ClusterCacheHandle chandle(ClusterCacheHandle(0));

    EXPECT_EQ(ClusterCacheMode::ContentBased,
              v->effective_cluster_cache_mode());

    EXPECT_EQ(chandle,
              v->getClusterCacheHandle());

    v->set_cluster_cache_mode(ClusterCacheMode::LocationBased);
    EXPECT_EQ(ClusterCacheMode::LocationBased,
              v->effective_cluster_cache_mode());

    const ClusterCacheHandle handle(v->getClusterCacheHandle());
    EXPECT_NE(chandle,
              handle);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    ClusterCache& cc = VolManager::get()->getClusterCache();

    const ClusterCache::NamespaceInfo cnsinfo(cc.namespace_info(handle));
    EXPECT_EQ(handle,
              cnsinfo.handle);

    localRestart(ns->ns());
    v = getVolume(VolumeId(ns->ns().str()));

    EXPECT_EQ(handle,
              v->getClusterCacheHandle());

    const VolumeConfig cfg(v->get_config());

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    EXPECT_THROW(cc.namespace_info(handle),
                 std::exception);

    restartVolume(cfg);
    v = getVolume(VolumeId(ns->ns().str()));

    EXPECT_NE(chandle,
              v->getClusterCacheHandle());

    EXPECT_EQ(ClusterCacheMode::LocationBased,
              v->effective_cluster_cache_mode());
}

TEST_P(SimpleVolumeTest, sco_multiplier_updates)
{
    auto wrns(make_random_namespace());
    Volume* v = newVolume(*wrns);

    auto fun([&](const SCOMultiplier sm,
                 const std::string& pattern) -> SCONumber
             {
                 {
                     fungi::ScopedLock l(api::getManagementMutex());
                     v->setSCOMultiplier(sm);
                     EXPECT_EQ(sm,
                               v->getSCOMultiplier());
                 }

                 DataStoreNG& ds = *v->getDataStore();

                 EXPECT_EQ(sm.t,
                           ds.getRemainingSCOCapacity());

                 writeToVolume(v,
                               0,
                               v->getClusterSize() * (sm.t - 1),
                               pattern);

                 return ds.getCurrentSCONumber();
             });

    const SCONumber sco_num1 = fun(SCOMultiplier(1024),
                                   "one");
    const SCONumber sco_num2 = fun(SCOMultiplier(512),
                                   "two");
    const SCONumber sco_num3 = fun(SCOMultiplier(2048),
                                   "three");

    EXPECT_LT(sco_num1, sco_num2);
    EXPECT_LT(sco_num2, sco_num3);

    checkVolume(v,
                0,
                v->getClusterSize() * 2047,
                "three");
}

INSTANTIATE_TEST(SimpleVolumeTest);

}

// Local Variables: **
// mode: c++ **
// End: **
