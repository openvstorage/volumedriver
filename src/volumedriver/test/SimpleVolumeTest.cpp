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

#include "VolManagerTestSetup.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/filesystem/fstream.hpp>
#include <boost/scope_exit.hpp>

#include <youtils/Assert.h>
#include <youtils/FileUtils.h>

#include <backend/BackendInterface.h>

#include <volumedriver/Api.h>
#include <volumedriver/DataStoreNG.h>
#include <volumedriver/VolManager.h>
#include <volumedriver/VolumeConfig.h>
#include <volumedriver/VolumeConfigPersistor.h>
#include <volumedriver/VolumeThreadPool.h>

namespace volumedrivertest
{

using namespace initialized_params;
using namespace volumedriver;
namespace bpt = boost::property_tree;
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
                                                    const SnapshotName& snap)> fun)
    {
        auto ns(make_random_namespace());
        SharedVolumePtr v = newVolume(*ns);

        const std::string pattern("some data");
        writeToVolume(*v,
                      Lba(0),
                      v->getClusterSize(),
                      pattern);

        const SnapshotName snap("snap");

        SCOPED_BLOCK_BACKEND(*v);

        createSnapshot(*v,
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

        sm.cloneSnapshotPersistor().saveToFile(p,
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
        VolManager::get()->volumePotential(default_cluster_size(),
                                           default_sco_multiplier(),
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
            VolManager::get()->volumePotential(default_cluster_size(),
                                               default_sco_multiplier(),
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
    SharedVolumePtr v = newVolume(*wrns);

    const boost::optional<TLogMultiplier> tm(v->getTLogMultiplier());
    const TLogMultiplier tm_eff(v->getEffectiveTLogMultiplier());

    fungi::ScopedLock l(api::getManagementMutex());

    const uint64_t pot = volume_potential_sco_cache(v->getClusterSize(),
                                                    v->getSCOMultiplier(),
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
    SharedVolumePtr v = newVolume(*wrns);

    fungi::ScopedLock l(api::getManagementMutex());

    // current min: 1 MiB
    const SCOMultiplier min((1ULL << 20) / default_cluster_size());

    EXPECT_THROW(v->setSCOMultiplier(SCOMultiplier(min - 1)),
                 fungi::IOException);

    v->setSCOMultiplier(min);
    EXPECT_EQ(min,
              v->getSCOMultiplier());

    // current max: 128 MiB
    const SCOMultiplier max((128ULL << 20) / default_cluster_size());
    EXPECT_THROW(v->setSCOMultiplier(SCOMultiplier(max + 1)),
                 fungi::IOException);

    const SCOMultiplier sm(SCOMultiplier(v->getSCOMultiplier() + 1));
    v->setSCOMultiplier(sm);
    EXPECT_EQ(sm,
              v->getSCOMultiplier());
}

TEST_P(SimpleVolumeTest, alignment)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns);

    ASSERT_TRUE(__alignof__(*v) >= 4);
}

TEST_P(SimpleVolumeTest, sizeTest)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();
    ASSERT_THROW(newVolume(VolumeId("volume1"),
                           ns,
                           VolumeSize(VolManager::get()->real_max_volume_size() + 1)),
                 fungi::IOException);

    const size_t size =
        (VolManager::get()->real_max_volume_size() / default_cluster_size()) * default_cluster_size();
    ASSERT_NO_THROW(newVolume(VolumeId("volume1"),
                              ns,
                              VolumeSize(size)));
}

struct VolumeWriter
{
    VolumeWriter(SharedVolumePtr v)
        :vol_(v)
        , stop_(false)
    {}

    void
    operator()()
    {
        while(not stop_)
        {
            VolManagerTestSetup::writeToVolume(*vol_,Lba(0), 4096,"blah");
            usleep(100);
        }
    }

    void
    stop()
    {
        stop_ = true;
    }


    SharedVolumePtr vol_;
    bool stop_;
};

// disabled as of VOLDRV-1015 due to partial pointlessness
#if 0
TEST_P(SimpleVolumeTest, DISABLED_dumpMDStore)
{
    VolumeId vid("volume1");

    SharedVolumePtr v = newVolume(vid,
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
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
			  ns);

    const std::string pattern("blah");

    ASSERT_TRUE(v != nullptr);
    writeToVolume(*v,
                  Lba(0),
                  4096,
                  pattern);

    checkVolume(*v,
                Lba(0),
                4096,
                pattern);

    v->halt();

    EXPECT_THROW(writeToVolume(*v,
                               Lba(0),
                               4096,
                               "fubar"),
                 std::exception);

    EXPECT_THROW(checkVolume(*v,
                             Lba(0),
                             4096,
                             pattern),
                 std::exception);
}

TEST_P(SimpleVolumeTest, test1)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
			  ns);
    ASSERT_TRUE(v != nullptr);
    writeToVolume(*v,
                  Lba(0),
                  4096,
                  "immanuel");
    writeToVolume(*v,
                  Lba(4096),
                  4096,
                  "joost");

    writeToVolume(*v,
                  Lba(2*4096),
                  4096,
                  "arne");

    checkVolume(*v,
                Lba(0),
                4096,
                "immanuel");
    checkVolume(*v,
                Lba(4096),
                4096,
                "joost");
    checkVolume(*v,
                Lba(2*4096),
                4096,
                "arne");
}

TEST_P(SimpleVolumeTest, test2)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
			  ns);
    ASSERT_TRUE(v != nullptr);
    writeToVolume(*v,
                  Lba(0),
                  4096,
                  "immanuel");
    writeToVolume(*v,
                  Lba(0),
                  4096,
                  "joost");

    writeToVolume(*v,
                  Lba(0),
                  4096,
                  "arne");

    checkVolume(*v,
                Lba(0),
                4096,
                "arne");

}

TEST_P(SimpleVolumeTest, test3)
{
    // backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();


    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns);

    ASSERT_TRUE(v != nullptr);

    writeToVolume(*v,
                  Lba(0),
                  4096,
                  "immanuel");

    const SnapshotName snap1("snap1");

    v->createSnapshot(snap1);
    waitForThisBackendWrite(*v);
    waitForThisBackendWrite(*v);
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& ns_clone = ns1_ptr->ns();

    // backend::Namespace ns_clone;

    SharedVolumePtr c = 0;
    c = createClone("clone1",
                    ns_clone,
                    ns,
                    snap1);

    checkVolume(*c,
                Lba(0),
                4096,
                "immanuel");
}

TEST_P(SimpleVolumeTest, test4)
{
    SharedVolumePtr v = 0;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    //backend::Namespace ns1;
    v = newVolume(VolumeId("volume1"),
                  ns1);
    ASSERT_TRUE(v != nullptr);
    writeToVolume(*v,
                  Lba(0),
                  4096,
                  "a");

    const SnapshotName snap1("snap1");
    v->createSnapshot(snap1);

    waitForThisBackendWrite(*v);
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& ns_clone1 = ns1_ptr->ns();

    // backend::Namespace ns_clone1;

    SharedVolumePtr c1 = 0;
    c1 = createClone("clone1",
                     ns_clone1,
                     ns1,
                     snap1);
    writeToVolume(*c1,
                  Lba(4096),
                  4096,
                  "b");

    checkVolume(*c1,
                Lba(0),
                4096,
                "a");


    checkVolume(*c1,
                Lba(4096),
                4096,
                "b");

    const SnapshotName snap2("snap2");
    c1->createSnapshot(snap2);

    waitForThisBackendWrite(*c1);
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns_clone2 = ns2_ptr->ns();

    // backend::Namespace ns_clone2;
    SharedVolumePtr c2 = 0;
    c2 = createClone("clone2",
                     ns_clone2,
                     ns_clone1,
                     snap2);
    ASSERT_TRUE(c2 != nullptr);

     checkVolume(*c2,
                 Lba(4096),
                 4096,
                 "b");
    checkVolume(*c2,
                Lba(0),
                4096,
                "a");


    writeToVolume(*c2,
                  Lba(2*4096),
                  4096,
                  "c");
    checkVolume(*c2,
                Lba(2*4096),
                4096,
                "c");

    const SnapshotName snap3("snap3");
    c2->createSnapshot(snap3);
    waitForThisBackendWrite(*c2);

    SharedVolumePtr c3 = 0;
    auto ns3_ptr = make_random_namespace();

    const backend::Namespace& ns_clone3 = ns3_ptr->ns();

    // backend::Namespace ns_clone3;
    c3 = createClone("clone3",
                     ns_clone3,
                     ns_clone2,
                     snap3);

    ASSERT_TRUE(c3 != nullptr);

    checkVolume(*c3,
                Lba(0),
                4096,
                "a");

    checkVolume(*c3,
                Lba(4096),
                4096,
                "b");
    checkVolume(*c3,
                Lba(2*4096),
                4096,
                "c");
    writeToVolume(*c3,
                  Lba(3*4096),
                  4096,
                  "d");

    checkVolume(*c3,
                Lba(3*4096),
                4096,
                "d");

    const SnapshotName snap4("snap4");
    c3->createSnapshot(snap4);
    waitForThisBackendWrite(*c3);

    SharedVolumePtr c4 = 0;
    auto ns4_ptr = make_random_namespace();

    const backend::Namespace& ns_clone4 = ns4_ptr->ns();

    //    backend::Namespace ns_clone4;
    c4 = createClone("clone4",
                     ns_clone4,
                     ns_clone3,
                     snap4);
    ASSERT_TRUE(c4 != nullptr);
}

TEST_P(SimpleVolumeTest, switchTLog)
{
    // difftime doesn't work for me.
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns);

    const TLogId init_tlog(v->getSnapshotManagement().getCurrentTLogId());
    std::vector<uint8_t> v1(4096);

    uint64_t clusters = VolManager::get()->number_of_scos_in_tlog.value() *  v->getSCOMultiplier();

    for(unsigned i = 0; i < clusters - 1; ++i) {
        writeToVolume(*v, Lba(0), 4096, &v1[0]);
    }

    ASSERT_EQ(init_tlog,
              v->getSnapshotManagement().getCurrentTLogId());

    writeToVolume(*v, Lba(0), 4096, &v1[0]);

    ASSERT_NE(init_tlog,
              v->getSnapshotManagement().getCurrentTLogId());
}

TEST_P(SimpleVolumeTest, restoreSnapshot)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    //const backend::Namespace nspace;
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          nspace);

    const int count = v->getClusterSize();

    const std::string snapshotName1("nikon");
    const std::string snapshotName2("canon");

    writeToVolume(*v, Lba(0), count, snapshotName1);
    writeToVolume(*v, Lba(0), count, snapshotName2);

    checkVolume(*v, Lba(0), count, snapshotName2);

    {
        SCOPED_BLOCK_BACKEND(*v);
        ASSERT_NO_THROW(createSnapshot(*v, snapshotName1));
        writeToVolume(*v, Lba(0), count, snapshotName1);

        EXPECT_THROW(createSnapshot(*v, snapshotName2), PreviousSnapshotNotOnBackendException);
        writeToVolume(*v, Lba(0), count,"bbbbb");

        EXPECT_THROW(restoreSnapshot(*v, snapshotName1), fungi::IOException);


    }
    waitForThisBackendWrite(*v);

    {
        SCOPED_BLOCK_BACKEND(*v);
        ASSERT_NO_THROW(createSnapshot(*v, snapshotName2));
        writeToVolume(*v, Lba(0), count,"ccccc");
    }
    waitForThisBackendWrite(*v);


    EXPECT_NO_THROW(restoreSnapshot(*v, snapshotName1));


    checkVolume(*v, Lba(0),count,snapshotName2);

    // we should be able to rewrite now:
    writeToVolume(*v, Lba(0), count,"aaaaa");

    checkVolume(*v, Lba(0),count,"aaaaa");

    // make sure tlogs are not leaked
    waitForThisBackendWrite(*v);

    std::list<std::string> tlogsInBackend;
    backendRegex(nspace,
                "tlog_.*",
                tlogsInBackend);

    const OrderedTLogIds allTLogs(v->getSnapshotManagement().getAllTLogs());

    for (auto& tlog_name : tlogsInBackend)
    {
        for (const auto& tlog_id : allTLogs)
        {
            if (tlog_name == boost::lexical_cast<std::string>(tlog_id))
            {
                tlog_name = "";
                break;
            }
        }
    }

    for (const auto& tlog_name : tlogsInBackend)
    {
        EXPECT_EQ("", tlog_name) << "TLog " << tlog_name << " leaked in backend";
    }
}

TEST_P(SimpleVolumeTest, restoreSnapshot2)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          nspace);

    const int count = v->getClusterSize();

    const std::string snapshotName1("nikon");
    const std::string snapshotName2("canon");

    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);
    v->set_cluster_cache_mode(ClusterCacheMode::LocationBased);

    EXPECT_TRUE(v->isCacheOnRead());
    EXPECT_FALSE(v->isCacheOnWrite());

    writeToVolume(*v, Lba(0), count, snapshotName1);
    writeToVolume(*v, Lba(0), count, snapshotName2);

    checkVolume(*v, Lba(0), count, snapshotName2);

    {
        SCOPED_BLOCK_BACKEND(*v);
        ASSERT_NO_THROW(createSnapshot(*v, snapshotName1));

        v->set_cluster_cache_behaviour(ClusterCacheBehaviour::NoCache);
        EXPECT_TRUE(v->effective_cluster_cache_behaviour() == ClusterCacheBehaviour::NoCache);
        v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);
        EXPECT_TRUE(v->isCacheOnRead());
        EXPECT_FALSE(v->isCacheOnWrite());

        writeToVolume(*v, Lba(0), count, snapshotName1);

        EXPECT_THROW(createSnapshot(*v, snapshotName2), PreviousSnapshotNotOnBackendException);
        writeToVolume(*v, Lba(0), count,"cnanakos1");

        EXPECT_THROW(restoreSnapshot(*v, snapshotName1), fungi::IOException);
    }

    waitForThisBackendWrite(*v);

    {
        SCOPED_BLOCK_BACKEND(*v);
        ASSERT_NO_THROW(createSnapshot(*v, snapshotName2));

        v->set_cluster_cache_behaviour(ClusterCacheBehaviour::NoCache);
        EXPECT_TRUE(v->effective_cluster_cache_behaviour() == ClusterCacheBehaviour::NoCache);
        ASSERT_THROW(v->set_cluster_cache_mode(ClusterCacheMode::ContentBased),
                     fungi::IOException);
        v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);
        EXPECT_TRUE(v->effective_cluster_cache_behaviour() != ClusterCacheBehaviour::NoCache);
        EXPECT_TRUE(v->isCacheOnRead());
        EXPECT_FALSE(v->isCacheOnWrite());

        writeToVolume(*v, Lba(0), count,"cnanakos2");
    }

    waitForThisBackendWrite(*v);

    EXPECT_NO_THROW(restoreSnapshot(*v, snapshotName1));

    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::NoCache);
    EXPECT_TRUE(v->effective_cluster_cache_behaviour() == ClusterCacheBehaviour::NoCache);
    ASSERT_THROW(v->set_cluster_cache_mode(ClusterCacheMode::ContentBased),
                 fungi::IOException);
    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);
    EXPECT_TRUE(v->isCacheOnRead());
    EXPECT_FALSE(v->isCacheOnWrite());

    checkVolume(*v, Lba(0),count,snapshotName2);

    writeToVolume(*v, Lba(0), count,"cnanakos4");

    checkVolume(*v, Lba(0),count,"cnanakos4");

    waitForThisBackendWrite(*v);

    std::list<std::string> tlogsInBackend;
    backendRegex(nspace,
                "tlog_.*",
                tlogsInBackend);

    const OrderedTLogIds allTLogs(v->getSnapshotManagement().getAllTLogs());

    for (auto& tlog_name : tlogsInBackend)
    {
        for (const auto& tlog_id : allTLogs)
        {
            if (tlog_name == boost::lexical_cast<std::string>(tlog_id))
            {
                tlog_name = "";
                break;
            }
        }
    }

    for (const auto& tlog_name : tlogsInBackend)
    {
        EXPECT_EQ("", tlog_name) << "TLog " << tlog_name << " leaked in backend";
    }
}

TEST_P(SimpleVolumeTest, restoreSnapshotWithFuckedUpTLogs)
{
    uint32_t scosInTlog    = 1;
    SCOMultiplier clustersInSco = SCOMultiplier(4);

    //    SnapshotManagement::setNumberOfSCOSBetweenCheck(scosInTlog);
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns,
                          VolumeSize((1 << 18) * 512),
                          clustersInSco);

    const int count = v->getClusterSize();

    const std::string snapshotName1("nikon1");

    writeToVolume(*v, Lba(0), count, snapshotName1);

    createSnapshot(*v, snapshotName1);
    waitForThisBackendWrite(*v);

    //make sure we delete some tlog in the middle, not just the first one
    for (uint32_t i = 0 ; i < 5 * clustersInSco * scosInTlog; i++)
    {
        writeToVolume(*v,
                      Lba(0),
                      count,
                      "there are no rules without exceptions, so there are rules without exceptions");
    }

    const TLogId tlog_id(v->getSnapshotManagement().getCurrentTLogId());

    for (uint32_t i = 0 ; i < 5 * clustersInSco * scosInTlog; i++)
    {
        writeToVolume(*v,
                      Lba(0),
                      count,
                      "there are no rules without exceptions, so there are rules without exceptions");
    }

    const std::string snapshotName2("nikon2");

    createSnapshot(*v, snapshotName2);
    waitForThisBackendWrite(*v);

    v->getBackendInterface()->remove(boost::lexical_cast<std::string>(tlog_id));

    ASSERT_NO_THROW(restoreSnapshot(*v, snapshotName1));
    checkVolume(*v, Lba(0),count,snapshotName1);
}

TEST_P(SimpleVolumeTest, restoreSnapshotWithFuckedUpNeededTLogs)
{
    uint32_t scosInTlog    = 1;
    SCOMultiplier clustersInSco = SCOMultiplier(4);

    //SnapshotManagement::setNumberOfSCOSBetweenCheck(scosInTlog);
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns,
                          VolumeSize((1 << 18) * 512),
                          clustersInSco);

    const int count = v->getClusterSize();

    const std::string snapshotName1("nikon1");

    //make sure we delete some tlog in the middle, not just the first one
    for (uint32_t i = 0 ; i < 5 * clustersInSco * scosInTlog; i++) {
        writeToVolume(*v, Lba(0), count, "should be overwritten");
    }

    const TLogId tlog_id = v->getSnapshotManagement().getCurrentTLogId();

    for (uint32_t i = 0 ; i < 5 * clustersInSco * scosInTlog; i++)
    {
        writeToVolume(*v, Lba(0), count, "should be overwritten");
    }

    writeToVolume(*v, Lba(0), count, snapshotName1);

    for (uint32_t i = 0 ; i < 5 * clustersInSco * scosInTlog; i++)
    {
        writeToVolume(*v, Lba(0), count, "there are no rules without exceptions, so there are rules without exceptions");
    }

    createSnapshot(*v, snapshotName1);
    waitForThisBackendWrite(*v);

    const std::string snapshotName2("nikon2");

    createSnapshot(*v, snapshotName2);
    waitForThisBackendWrite(*v);

    v->getBackendInterface()->remove(boost::lexical_cast<std::string>(tlog_id));

    ASSERT_THROW(restoreSnapshot(*v, snapshotName1), std::exception);
}


TEST_P(SimpleVolumeTest, dontLeakStorageOnSnapshotRollback)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    //const Namespace nspace;
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          nspace);

    const int size = v->getClusterSize();

    const std::string pattern1 = "sco1";
    writeToVolume(*v, Lba(0), size, pattern1);
    createSnapshot(*v, "snap1");

    waitForThisBackendWrite(*v);

    const std::string pattern2 = "sco2";
    writeToVolume(*v, Lba(0), size, pattern2);
    createSnapshot(*v, "snap2");

    waitForThisBackendWrite(*v);

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

    ASSERT_NO_THROW(restoreSnapshot(*v, "snap1"));

    scos.clear();
    tlogs.clear();
    waitForThisBackendWrite(*v);

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
    SharedVolumePtr v = 0;
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

    ASSERT_TRUE(v != nullptr);

    ASSERT_NO_THROW(destroyVolume(v,
                                  DeleteLocalData::T,
                                  RemoveVolumeCompletely::T));
}


TEST_P(SimpleVolumeTest, reCreateVol)
{
    SharedVolumePtr v1 = nullptr;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    ASSERT_NO_THROW(v1 = newVolume(VolumeId("volume1"),
				   ns));
    ASSERT_TRUE(v1 != nullptr);

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

    SharedVolumePtr v = 0;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    ASSERT_NO_THROW(v = newVolume(volname,
				  ns));
    ASSERT_TRUE(v != nullptr);

    const SnapshotName snap("snap1");
    ASSERT_NO_THROW(v->createSnapshot(snap));

    while(not isVolumeSyncedToBackend(*v))
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
    SharedVolumePtr v1 = newVolume("1volume",
                           ns);
    {
        SCOPED_BLOCK_BACKEND(*v1);
        v1->createSnapshot(snap);

        EXPECT_THROW(v1->restoreSnapshot(snap),
                     fungi::IOException);



    }
    waitForThisBackendWrite(*v1);
    waitForThisBackendWrite(*v1);

    EXPECT_NO_THROW(v1->restoreSnapshot(snap));

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(SimpleVolumeTest, RollBack2)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume("1volume",
			   ns);

    const SnapshotName snap("snap1");
    v1->createSnapshot(snap);
    waitForThisBackendWrite(*v1);

    {
        SCOPED_BLOCK_BACKEND(*v1);
        v1->createSnapshot(SnapshotName("snap2"));
    }

    v1->restoreSnapshot(snap);

    waitForThisBackendWrite(*v1);
    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(SimpleVolumeTest, LocalRestartClone)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    //    backend::Namespace ns1;

    SharedVolumePtr v1 = newVolume("1volume",
			   ns1);
    for(int i = 0; i < 10; i++)
    {
        writeToVolume(*v1,
                      Lba(0),
                      4096,
                      "immanuel");
    }

    const SnapshotName snap1("snap1");
    v1->createSnapshot(snap1);
    waitForThisBackendWrite(*v1);
    waitForThisBackendWrite(*v1);

    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& clone_ns = ns1_ptr->ns();

    // backend::Namespace clone_ns;

    SharedVolumePtr c = createClone("clone1",
                            clone_ns,
                            ns1,
                            snap1);
    destroyVolume(c,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    ASSERT_NO_THROW(localRestart(clone_ns));

    c = 0;
    c = getVolume(VolumeId("clone1"));
    ASSERT_TRUE(c != nullptr);

    checkVolume(*c,
                Lba(0),
                4096,
                "immanuel");
}

TEST_P(SimpleVolumeTest, RollBackClone)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    // const backend::Namespace ns1;

    SharedVolumePtr v1 = newVolume("1volume",
			   ns1);
    const int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 50; i++)
    {

        writeToVolume(*v1,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "immanuel");
    }

    const SnapshotName snap1("snap1");

    v1->createSnapshot(snap1);
    waitForThisBackendWrite(*v1);
    waitForThisBackendWrite(*v1);

    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    // const backend::Namespace ns2;

    SharedVolumePtr v2 = createClone("2volume",
                             ns2,
                             ns1,
                             snap1);

    auto ns3_ptr = make_random_namespace();

    const backend::Namespace& ns3 = ns3_ptr->ns();

    // const backend::Namespace ns3;
    SharedVolumePtr v3 = newVolume("3volume",
                           ns3);

    for(int i = 50; i < 100; i++)
    {
        writeToVolume(*v2,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "arne");
        writeToVolume(*v3,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "arne");
        writeToVolume(*v1,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "arne");
    }

    EXPECT_FALSE(checkVolumesIdentical(*v2,
                                       *v3));
    EXPECT_TRUE(checkVolumesIdentical(*v1,
                                      *v2));
    v2->createSnapshot(snap1);

    for(int i = 50; i < 100; i++)
    {
        writeToVolume(*v2,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "kristaf");
    }

    EXPECT_FALSE(checkVolumesIdentical(*v3,
                                       *v2));
    EXPECT_FALSE(checkVolumesIdentical(*v1,
                                       *v2));
    waitForThisBackendWrite(*v2);
    waitForThisBackendWrite(*v2);

    v2->restoreSnapshot(snap1);

    EXPECT_FALSE(checkVolumesIdentical(*v2,
                                       *v3));
    EXPECT_TRUE(checkVolumesIdentical(*v2,
                                      *v1));
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

    SharedVolumePtr vol = newVolume("vol1",
			    backend::Namespace());
    VolumeConfig cfg = vol->get_config();
    //struct rusage rus;
    for(int i = 0; i < 20; ++i)
    {
        SharedVolumePtr v = getVolume(VolumeId("vol1"));
        ASSERT_TRUE(v != nullptr);
        LOG_INFO("Run " << i);
        size_t upto = 50;

        for(size_t j = 0; j < upto -1; ++j)
        {
            std::stringstream ss;
            ss << (i*upto) + j;
            createSnapshot(*v,ss.str());
        }
        LOG_INFO("Creating ze bigsnap");

        std::stringstream ss;
        ss << "BigSnap_" << i;

        gettimeofday(&start, 0);
        createSnapshot(*v,ss.str());
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

    SharedVolumePtr v1 = newVolume(VolumeId("volume1"),
			   ns);

    const SnapshotName snap("snap1");

    EXPECT_THROW(v1->isSyncedToBackendUpTo(snap),
                 fungi::IOException);

    {
        SCOPED_BLOCK_BACKEND(*v1);

            v1->createSnapshot(snap);
            EXPECT_FALSE(v1->isSyncedToBackendUpTo(snap));
    }

    waitForThisBackendWrite(*v1);
    EXPECT_TRUE(v1->isSyncedToBackendUpTo(snap));
}

TEST_P(SimpleVolumeTest, backend_sync_up_to_tlog)
{
    auto ns(make_random_namespace());
    SharedVolumePtr v = newVolume(*ns);

    const SnapshotManagement& sm = v->getSnapshotManagement();
    const auto old_tlog_id(sm.getCurrentTLogId());

    EXPECT_FALSE(v->isSyncedToBackendUpTo(old_tlog_id));

    TLogId new_tlog_id;

    {
        SCOPED_BLOCK_BACKEND(*v);

        EXPECT_EQ(old_tlog_id,
                  v->scheduleBackendSync());

        new_tlog_id = sm.getCurrentTLogId();

        EXPECT_NE(old_tlog_id,
                  new_tlog_id);

        EXPECT_FALSE(v->isSyncedToBackendUpTo(old_tlog_id));
        EXPECT_FALSE(v->isSyncedToBackendUpTo(new_tlog_id));
    }

    waitForThisBackendWrite(*v);

    EXPECT_TRUE(v->isSyncedToBackendUpTo(old_tlog_id));
    EXPECT_FALSE(v->isSyncedToBackendUpTo(new_tlog_id));
}

TEST_P(SimpleVolumeTest, consistency)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(VolumeId("volume1"),
			   ns);
    const int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 1024; i++)
    {

        writeToVolume(*v1,
                      Lba(i*mult),
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

    SharedVolumePtr v1 = newVolume(VolumeId("volume1"),
			   ns1);
    const int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < (1024 ); i++)
    {

        writeToVolume(*v1,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "kristaf");
    }

    const SnapshotName snap1("snap1");
    v1->createSnapshot(snap1);
    waitForThisBackendWrite(*v1);

    // backend::Namespace ns2;
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    SharedVolumePtr v2 = createClone("volume2",
                             ns2,
                             ns1,
                             snap1);
    for(int i = 0; i < (1024 ); i++)
    {
        writeToVolume(*v1,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "kristaf");

        writeToVolume(*v2,
                      Lba(i*mult + 20),
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

    SharedVolumePtr v1 = newVolume(VolumeId("volume1"),
                           ns1);
    writeToVolume(*v1,
                  Lba(0),
                  v1->getClusterSize(),
                  "kristafke");

    fungi::ScopedLock l(api::getManagementMutex());
    uint64_t  hits =  api::getClusterCacheHits(VolumeId("volume1"));
    uint64_t  misses =  api::getClusterCacheMisses(VolumeId("volume1"));
    EXPECT_EQ(0U, hits);
    EXPECT_EQ(0U, misses);
    checkVolume(*v1, Lba(0), v1->getClusterSize(), "kristafke");
    hits =  api::getClusterCacheHits(VolumeId("volume1"));
    misses =  api::getClusterCacheMisses(VolumeId("volume1"));
    EXPECT_EQ(0U, hits);
    EXPECT_EQ(1U, misses);

    checkVolume(*v1, Lba(0), v1->getClusterSize(), "kristafke");
    hits =  api::getClusterCacheHits(VolumeId("volume1"));
    misses =  api::getClusterCacheMisses(VolumeId("volume1"));
#ifdef ENABLE_MD5_HASH
    EXPECT_EQ(1U, hits);
    EXPECT_EQ(1U, misses);
#else
    EXPECT_EQ(0U, hits);
    EXPECT_EQ(2U, misses);
#endif

    checkVolume(*v1, Lba(0), v1->getClusterSize(), "kristafke");
    hits =  api::getClusterCacheHits(VolumeId("volume1"));
    misses =  api::getClusterCacheMisses(VolumeId("volume1"));
#ifdef ENABLE_MD5_HASH
    EXPECT_EQ(2U, hits);
    EXPECT_EQ(1U, misses);
#else
    EXPECT_EQ(0U, hits);
    EXPECT_EQ(3U, misses);
#endif

    checkVolume(*v1, Lba(0), v1->getClusterSize(), "kristafke");
    hits =  api::getClusterCacheHits(VolumeId("volume1"));
    misses =  api::getClusterCacheMisses(VolumeId("volume1"));
#ifdef ENABLE_MD5_HASH
    EXPECT_EQ(3U, hits);
    EXPECT_EQ(1U, misses);
#else
    EXPECT_EQ(0U, hits);
    EXPECT_EQ(4U, misses);
#endif
}


TEST_P(SimpleVolumeTest, backendSize)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(VolumeId("volume1"),
			   ns1);
    const unsigned int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 1024 ; i++)
    {

        writeToVolume(*v1,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "kristaf");
    }
    waitForThisBackendWrite(*v1);

    uint64_t size = 0;
    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 1024 * v1->getClusterSize());

    const SnapshotName snap("testsnap");

    {
        SCOPED_BLOCK_BACKEND(*v1);
        v1->createSnapshot(snap);
        size = 0;
        ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
        EXPECT_TRUE(size == 0);
        size = 0;
        ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap));
        EXPECT_TRUE(size == 1024 * v1->getClusterSize());

    }
    waitForThisBackendWrite(*v1);

    ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap));

    EXPECT_TRUE(size == 1024 * v1->getClusterSize());
    for(int i = 0; i < 1024 ; i++)
    {
        writeToVolume(*v1,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "kristaf");
    }
    waitForThisBackendWrite(*v1);
    size =  0;
    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 1024 * v1->getClusterSize());
}

TEST_P(SimpleVolumeTest, backendSize2)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(VolumeId("volume1"),
                           ns1);
    uint64_t size =  0;
    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 0);

    const SnapshotName snap("testsnap");
    {
        SCOPED_BLOCK_BACKEND(*v1);

        v1->createSnapshot(snap);
        size = 0;
        ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap));
        EXPECT_TRUE(size == 0);
    }


    waitForThisBackendWrite(*v1);
    size = 0;
    ASSERT_NO_THROW(size = v1->getSnapshotBackendSize(snap));
    EXPECT_TRUE(size == 0);
    const unsigned int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 1024 ; i++)
    {
        writeToVolume(*v1,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "kristaf");
    }
    waitForThisBackendWrite(*v1);

    size =  0;
    ASSERT_NO_THROW(size = v1->getCurrentBackendSize());
    EXPECT_TRUE(size == 1024 * v1->getClusterSize());
}

TEST_P(SimpleVolumeTest, backendSize3)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(VolumeId("volume1"),
			   ns1);
    const unsigned int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 1024 ; i++)
    {

        writeToVolume(*v1,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "kristaf");
    }

    const SnapshotName snap("testsnap");

    v1->createSnapshot(snap);
    waitForThisBackendWrite(*v1);
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

        writeToVolume(*v1,
                      Lba(i*mult),
                      v1->getClusterSize(),
                      "joost");
    }
    waitForThisBackendWrite(*v1);

    const SnapshotName snap2("testsnap2");
    v1->createSnapshot(snap2);
    waitForThisBackendWrite(*v1);

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

    SharedVolumePtr v1 = newVolume("v1",
			   ns1);

    be::BackendInterfacePtr bi(VolManager::get()->createBackendInterface(ns1));
    ASSERT_NO_THROW(VolumeConfigPersistor::load(*bi));
    ASSERT_NO_THROW(writeVolumeConfigToBackend(*v1));

    const VolumeConfig f(VolumeConfigPersistor::load(*bi));

    const VolumeConfig cf1 = v1->get_config();
    EXPECT_TRUE(cf1.id_ == f.id_);
    EXPECT_TRUE(cf1.getNS() == f.getNS());
    EXPECT_TRUE(cf1.parent_ns_ ==  f.parent_ns_);
    EXPECT_EQ(cf1.parent_snapshot_, f.parent_snapshot_);
    EXPECT_TRUE(cf1.lba_size_ == f.lba_size_);
    EXPECT_EQ(cf1.lba_count(), f.lba_count());
    EXPECT_TRUE(cf1.cluster_mult_ == f.cluster_mult_);
    EXPECT_TRUE(cf1.sco_mult_ == f.sco_mult_);
}

TEST_P(SimpleVolumeTest, readActivity)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume("v1",
			   ns1);
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    SharedVolumePtr v2 = newVolume("v2",
			   ns2);
    ASSERT_EQ(0, v1->readActivity());
    ASSERT_EQ(0, v2->readActivity());
    ASSERT_EQ(0, VolManager::get()->readActivity());

    boost::this_thread::sleep_for(boost::chrono::seconds(1));

    {
        fungi::ScopedLock l(VolManager::get()->getLock_());
        VolManager::get()->updateReadActivity();
    }

    ASSERT_EQ(0, v1->readActivity());
    ASSERT_EQ(0, v2->readActivity());
    ASSERT_EQ(0, VolManager::get()->readActivity());

    std::vector<uint8_t> buf(2 * default_cluster_size());

    for(int i = 0; i < 128; ++i)
    {
        v1->read(Lba(0), buf.data(), buf.size() / 2);
        v2->read(Lba(0), buf.data(), buf.size());
    }

    boost::this_thread::sleep_for(boost::chrono::seconds(1));

    {
        fungi::ScopedLock l(VolManager::get()->getLock_());
        VolManager::get()->updateReadActivity();
    }

    double ra1 = v1->readActivity();
    double ra2 = v2->readActivity();
    double rac = VolManager::get()->readActivity();

    ASSERT_GT(ra1, 0);
    ASSERT_GT(ra2, 0);
    ASSERT_EQ(ra2, 2 * ra1);
    ASSERT_EQ(rac, ra1 + ra2);
}

TEST_P(SimpleVolumeTest, prefetch)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    // const backend::Namespace nspace;
    const VolumeId volname("volume");

    SharedVolumePtr v = newVolume(volname,
			  nspace);

    ASSERT_TRUE(v != nullptr);

    VolManager* vm = VolManager::get();

    ASSERT_NO_THROW(updateReadActivity());
    persistXVals(volname);

    VolumeConfig cfg = v->get_config();

    const unsigned num_scos = 7;
    const uint64_t size = cfg.lba_size_ * cfg.cluster_mult_ * cfg.sco_mult_ * num_scos;
    const std::string pattern("prefetchin'");
    writeToVolume(*v,
                  Lba(0),
                  size,
                  pattern);

    std::vector<uint8_t> buf(size);


    for(int i =0 ; i< 64; ++i)
    {

        v->read(Lba(0), &buf[0], size);
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
    ASSERT_TRUE(v != nullptr);
    sleep(1);

    waitForPrefetching(*v);

    EXPECT_TRUE(c->hasNamespace(nspace));

    l.clear();
    c->getSCONameList(nspace,
                      l,
                      true);

    EXPECT_EQ(num_scos, l.size());
    EXPECT_LT(0U, vm->readActivity());

    checkVolume(*v,
                Lba(0),
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

    SharedVolumePtr v = newVolume(vid,
                          ns1);
    const std::string pattern("prefetchin'");
    EXPECT_NO_THROW(v->startPrefetch());
    VolumeConfig cfg = v->get_config();

    const int num_scos = 7;
    const uint64_t size = cfg.lba_size_ * cfg.cluster_mult_ * cfg.sco_mult_ * num_scos;

    writeToVolume(*v,
                  Lba(0),
                  size,
                  pattern);

    ASSERT_NO_THROW(updateReadActivity());
    persistXVals(vid);
    waitForThisBackendWrite(*v);

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

    SharedVolumePtr v = newVolume(name, ns);

    ASSERT_TRUE(v != nullptr);

    writeToVolume(*v,
                  Lba(0),
                  v->getClusterSize(),
                  "abcd");

    // required to make the TLog roll over
    sleep(1);

    writeToVolume(*v,
                  Lba(v->getClusterSize() / v->getLBASize()),
                  v->getClusterSize() * v->getSCOMultiplier(),
                  "defg");


    v->scheduleBackendSync();
    waitForThisBackendWrite(*v);

    EXPECT_TRUE(v->checkConsistency());
}
#if 0
TEST_P(SimpleVolumeTest, replicationID)
{
    newVolume(VolumeId("a"),
              backend::Namespace());
    SharedVolumePtr b = newVolume(VolumeId("b"),
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

    SharedVolumePtr v = newVolume(vname, ns, VolumeSize(0));
    EXPECT_TRUE(v != nullptr);

    EXPECT_EQ(0U, v->getLBACount());
    EXPECT_EQ(0U, v->getSize());

    uint8_t pattern = 0xab;
    const std::vector<uint8_t> wbuf(v->getClusterSize(), pattern);
    EXPECT_THROW(v->write(Lba(0), &wbuf[0], wbuf.size()),
                 std::exception);

    ++pattern;
    std::vector<uint8_t> rbuf(v->getClusterSize(), pattern);
    EXPECT_THROW(v->read(Lba(0), &rbuf[0], rbuf.size()),
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
    SharedVolumePtr v = newVolume(vname,
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

    SharedVolumePtr v = newVolume(vname,
                          ns);

    EXPECT_TRUE(v != nullptr);
    const uint64_t vsize = v->getSize();
    const uint64_t csize = v->getClusterSize();

    const uint64_t nclusters = (vsize + csize) / csize;
    EXPECT_LT(vsize, nclusters * csize);

    v->resize(nclusters);

    EXPECT_EQ(nclusters * csize, v->getSize());

    const std::string pattern("extended play");
    const Lba lba((nclusters - 1) * csize / v->getLBASize());

    writeToVolume(*v, lba, csize, pattern);
    checkVolume(*v, lba, csize, pattern);
}

TEST_P(SimpleVolumeTest, grow_beyond_end)
{
    const std::string vname("volume");
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume(vname,
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

    SharedVolumePtr v = newVolume(vname,
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
        writeToVolume(*v, Lba(0), csize, "not of any importance");
    }

    // should not have any impact.
    v->sync();

    EXPECT_EQ(tlog1, sm.getCurrentTLogPath());

    for (unsigned i = sco_mult; i < tlog_entries; ++i)
    {
        EXPECT_EQ(tlog1, sm.getCurrentTLogPath());
        writeToVolume(*v, Lba(0), csize, "neither of importance");
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
            CLANG_ANALYZER_HINT_NON_NULL(e);
            EXPECT_TRUE(e->isLocation());
        }

        auto e = tr.nextAny();
        ASSERT_TRUE(e != nullptr);
        CLANG_ANALYZER_HINT_NON_NULL(e);
        EXPECT_TRUE(e->isSCOCRC());
    }

    auto e = tr.nextAny();
    ASSERT_TRUE(e != nullptr);
    CLANG_ANALYZER_HINT_NON_NULL(e);
    EXPECT_TRUE(e->isTLogCRC());

    EXPECT_EQ(nullptr, tr.nextAny());
}

TEST_P(SimpleVolumeTest, sco_cache_limits)
{
    //    const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
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

    SharedVolumePtr v = newVolume(id,
                          ns);

    const VolumeConfig cfg = v->get_config();

    const std::string pattern("Hullo?");
    writeToVolume(*v,
                  Lba(0),
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

    checkVolume(*v,
                Lba(0),
                4096,
                pattern);
}

// We don't have a test for that yet?
TEST_P(SimpleVolumeTest, no_snapshot_if_previous_snapshot_is_not_yet_in_the_backend)
{
    with_snapshot_not_in_backend([this](Volume& v,
                                        const SnapshotName& /* snap */)
                                 {
                                     ASSERT_THROW(createSnapshot(v,
                                                                 "no_snapshot"),
                                                  std::exception);
                                 });
}

TEST_P(SimpleVolumeTest, no_clone_if_snapshot_is_not_yet_in_the_backend)
{
    with_snapshot_not_in_backend([this](Volume& v,
                                        const SnapshotName& snap)
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
    SharedVolumePtr parent = newVolume(*parent_ns);

    ASSERT_EQ(ccmode,
              parent->effective_cluster_cache_mode());

    ASSERT_EQ(ccbehaviour,
              parent->effective_cluster_cache_behaviour());

    const std::string pattern("written-to-parent");
    writeToVolume(*parent,
                  Lba(0),
                  4096,
                  pattern);

    const SnapshotName snap("snap");
    createSnapshot(*parent,
                   snap);

    waitForThisBackendWrite(*parent);
    ASSERT_TRUE(parent->getSnapshotManagement().lastSnapshotOnBackend());

    const auto clone_ns(make_random_namespace());
    SharedVolumePtr clone = createClone(*clone_ns,
                                        parent_ns->ns(),
                                        snap);

    ASSERT_EQ(ccmode,
              clone->effective_cluster_cache_mode());

    ASSERT_EQ(ccbehaviour,
              clone->effective_cluster_cache_behaviour());

    checkVolume(*clone,
                Lba(0),
                4096,
                pattern);
}

TEST_P(SimpleVolumeTest, cluster_cache_limit)
{
    auto ns(make_random_namespace());
    SharedVolumePtr v = newVolume(*ns);

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
    SharedVolumePtr v = newVolume(*ns);

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
    SharedVolumePtr v = newVolume(*wrns);

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

                 writeToVolume(*v,
                               Lba(0),
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

    checkVolume(*v,
                Lba(0),
                v->getClusterSize() * 2047,
                "three");
}

TEST_P(SimpleVolumeTest, synchronous_foc)
{
    auto wrns(make_random_namespace());

    SharedVolumePtr v = newVolume(*wrns);

    auto foc_ctx(start_one_foc());
    FailOverCacheConfig foc_config(foc_ctx->config(FailOverCacheMode::Synchronous));

    v->setFailOverCacheConfig(foc_config);

    ASSERT_EQ(VolumeFailOverState::OK_SYNC,
              v->getVolumeFailOverState());

    const std::string s("a single cluster");
    const size_t csize = v->getClusterSize();
    writeToVolume(*v,
                  Lba(0),
                  csize,
                  s);

    ASSERT_EQ(VolumeFailOverState::OK_SYNC,
              v->getVolumeFailOverState());

    FailOverCacheProxy proxy(foc_config,
                             wrns->ns(),
                             LBASize(v->getLBASize()),
                             v->getClusterMultiplier(),
                             boost::chrono::seconds(10),
                             boost::none);

    size_t count = 0;

    proxy.getEntries([&](ClusterLocation loc,
                         uint64_t lba,
                         const byte* buf,
                         size_t bufsize)
                     {
                         ++count;
                         EXPECT_EQ(ClusterLocation(1),
                                   loc);
                         EXPECT_EQ(0,
                                   lba);
                         EXPECT_EQ(csize,
                                   bufsize);

                         for (size_t i = 0; i < csize; ++i)
                         {
                             EXPECT_EQ(s[i % s.size()],
                                       buf[i]);
                         }
                     });

    EXPECT_EQ(1,
              count);
}

TEST_P(SimpleVolumeTest, metadata_cache_capacity)
{
    auto wrns(make_random_namespace());

    SharedVolumePtr v = newVolume(*wrns);

    auto check_cap([&](const boost::optional<size_t>& cap)
                   {
                       ASSERT_EQ(cap,
                                 v->get_config().metadata_cache_capacity_);

                       const VolumeConfig cfg(v->get_config());
                       ASSERT_EQ(cap,
                                 cfg.metadata_cache_capacity_);

                       if (not cap)
                       {
                           ASSERT_EQ(VolManager::get()->metadata_cache_capacity.value(),
                                     v->effective_metadata_cache_capacity());
                       }
                       else
                       {
                           ASSERT_EQ(*cap,
                                     v->effective_metadata_cache_capacity());
                       }
                   });

    check_cap(boost::none);

    const std::string fst("first");
    const size_t csize = v->getClusterSize();

    writeToVolume(*v,
                  Lba(0),
                  csize,
                  fst);

    syncToBackend(*v);

    const std::string snd("second");
    writeToVolume(*v,
                  Lba(csize),
                  csize,
                  snd);

    ASSERT_THROW(v->set_metadata_cache_capacity(0UL),
                 std::exception);

    auto check_([&](const boost::optional<size_t>& cap)
               {
                   check_cap(cap);

                   checkVolume(*v,
                               Lba(0),
                               csize,
                               fst);

                   checkVolume(*v,
                               Lba(csize),
                               csize,
                               snd);
               });

    check_(boost::none);

    auto check([&](const boost::optional<size_t>& cap)
               {
                    v->set_metadata_cache_capacity(cap);
                    check_(cap);
               });

    check(128);
    check(256);
    check(boost::none);
}

// cf. OVS-4033: the DTL queue depth was not taken into account
// when splitting write requests, so if a request > DTL queue
// depth was handled it would lead to infinite attempts sending
// the request's chunks over to the DTL.
TEST_P(SimpleVolumeTest, dtl_queue_depth_and_large_requests)
{
    const size_t qdepth = 1;
    {
        const PARAMETER_TYPE(dtl_queue_depth) qd(qdepth);
        bpt::ptree pt;
        api::persistConfiguration(pt, false);
        qd.persist(pt);
        api::updateConfiguration(pt);
    }

    {
        bpt::ptree pt;
        api::persistConfiguration(pt, false);
        const PARAMETER_TYPE(dtl_queue_depth) qd(pt);
        ASSERT_EQ(qdepth, qd.value());
    }

    auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    auto foc_ctx(start_one_foc());
    v->setFailOverCacheConfig(foc_ctx->config(FailOverCacheMode::Asynchronous));

    const size_t size = 10 * v->getClusterSize();

    const std::string pattern("not that interesting, really");

    writeToVolume(*v,
                  Lba(0),
                  size,
                  pattern);

    checkVolume(*v,
                Lba(0),
                size,
                pattern);
}

TEST_P(SimpleVolumeTest, partial_read_counters)
{
    auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    const std::string pattern("testing partial read counters");
    writeToVolume(*v, Lba(0), v->getClusterSize(), pattern);

    {
        const be::PartialReadCounter prc(v->getDataStore()->partial_read_counter());
        EXPECT_EQ(0UL, prc.fast + prc.slow);
    }

    const VolumeConfig cfg(v->get_config());
    v->scheduleBackendSync();
    waitForThisBackendWrite(*v);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    v = nullptr;
    restartVolume(cfg);
    v = getVolume(cfg.id_);
    ASSERT_NE(nullptr, v);

    checkVolume(*v, Lba(0), v->getClusterSize(), pattern);

    const be::PartialReadCounter prc(v->getDataStore()->partial_read_counter());
    EXPECT_LT(0UL, prc.fast + prc.slow);
}

TEST_P(SimpleVolumeTest, bytewise)
{
    auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    const size_t csize = v->getClusterSize();
    const std::string first("first");

    std::stringstream ss;
    const std::string s("second");

    for (size_t i = 0; i < csize + 1; i += s.size())
    {
        ss << s;
    }

    const std::string second(ss.str());
    const std::string third("3rd");

    v->write(0,
             reinterpret_cast<const uint8_t*>(first.data()),
             first.size());
    v->write(first.size(),
             reinterpret_cast<const uint8_t*>(second.data()),
             second.size());
    v->write(first.size() + second.size(),
             reinterpret_cast<const uint8_t*>(third.data()),
             third.size());

    auto check([&](const std::string& pattern,
                   size_t off)
               {
                   std::vector<char> rbuf(pattern.size());
                   v->read(off,
                           reinterpret_cast<uint8_t*>(rbuf.data()),
                           rbuf.size());
                   EXPECT_EQ(pattern,
                             std::string(rbuf.data(),
                                         rbuf.size()));
               });

    check(first, 0);
    check(second, first.size());
    check(third, first.size() + second.size());
    check("\0\0\0\0", first.size() + second.size() + third.size());
}

namespace
{

const ClusterMultiplier
big_cluster_multiplier(VolManagerTestSetup::default_test_config().cluster_multiplier() * 2);

const auto big_clusters_config = VolManagerTestSetup::default_test_config()
    .cluster_multiplier(big_cluster_multiplier);
}

INSTANTIATE_TEST_CASE_P(SimpleVolumeTests,
                        SimpleVolumeTest,
                        ::testing::Values(volumedriver::VolManagerTestSetup::default_test_config(),
                                          big_clusters_config));

}

// Local Variables: **
// mode: c++ **
// End: **
