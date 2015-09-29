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

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/foreach.hpp>

#include "VolManagerTestSetup.h"

#include "../Api.h"
#include "../SnapshotPersistor.h"
#include "../VolumeConfigParameters.h"

#include <fstream>

#include <youtils/Assert.h>

namespace volumedriver
{

#define LOCK_MGMT()                                             \
    fungi::ScopedLock __l(api::getManagementMutex())

class ApiTest
    : public VolManagerTestSetup
{
public:
    ApiTest()
        : VolManagerTestSetup("ApiTest")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }

    // simulates a *looong* volume restart.
    void
    fakeRestart(const Namespace& ns,
                boost::mutex* mutex,
                boost::condition_variable* cond,
                bool* run)
    {
        {
            boost::unique_lock<boost::mutex> l(*mutex);
            const VolumeConfig cfg(VanillaVolumeConfigParameters(VolumeId(ns.str()),
                                                                 ns,
                                                                 VolumeSize(1ULL << 20),
                                                                 new_owner_tag()));
            setNamespaceRestarting(ns, cfg);
            *run = true;
        }

        cond->notify_one();

        boost::unique_lock<boost::mutex> l(*mutex);


        while (*run)
        {
            cond->wait(l);
        }

        clearNamespaceRestarting(ns);
    }

    void
    writeAndCheckFirstCluster(Volume* vol,
                              byte pattern)
    {
        {
            const std::vector<byte> wbuf(4096, pattern);
            LOCK_MGMT();
            api::Write(vol, 0, &wbuf[0], wbuf.size());
        }

        {
            LOCK_MGMT();
            api::Sync(vol);
        }

        checkFirstCluster(vol, pattern);
    }

    void
    checkFirstCluster(Volume* vol,
                      byte pattern)
    {
        const std::vector<byte> ref(4096, pattern);
        std::vector<byte> rbuf(ref.size());

        {
            LOCK_MGMT();
            api::Read(vol, 0, &rbuf[0], rbuf.size());
        }

        EXPECT_TRUE(ref == rbuf);
    }

    void
    testApi(//const Namespace& ns,
            // const Namespace& clone_ns,
            std::string& snap)
    {
        auto ns1ptr = make_random_namespace();
        auto ns2ptr = make_random_namespace();

        const Namespace& ns = ns1ptr->ns();
        const Namespace& clone_ns = ns2ptr->ns();


        const VolumeId vol_id(ns.str());
        const VolumeSize vol_size(1 << 30);

        const OwnerTag parent_owner_tag(new_owner_tag());

        {
            LOCK_MGMT();
            ASSERT_NO_THROW(api::createNewVolume(VanillaVolumeConfigParameters(vol_id,
                                                                               ns,
                                                                               vol_size,
                                                                               parent_owner_tag)));
        }

        Volume* vol = 0;

        {
            LOCK_MGMT();
            ASSERT_NO_THROW(vol = api::getVolumePointer(vol_id));
        }

        writeAndCheckFirstCluster(vol, '1');

        {
            LOCK_MGMT();
            EXPECT_NO_THROW(snap = api::createSnapshot(vol_id,
                                                       SnapshotMetaData(),
                                                       &snap));
        }

        bool sync(false);
        do
        {
            sleep (1);
            LOCK_MGMT();
            EXPECT_NO_THROW(sync = api::isVolumeSynced(vol_id));
        }
        while (not sync);

        std::list<VolumeId> vol_list;
        {
            LOCK_MGMT();
            EXPECT_NO_THROW(api::getVolumeList(vol_list));
        }

        bool found = false;
        BOOST_FOREACH(VolumeId& i, vol_list)
        {
            if (i == vol_id)
            {
                found = true;
            }
        }

        EXPECT_TRUE(found);

        std::list<std::string> snap_list;
        {
            LOCK_MGMT();
            EXPECT_NO_THROW(api::showSnapshots(vol_id, snap_list));
        }

        EXPECT_EQ((unsigned)1, snap_list.size());
        EXPECT_EQ(snap, snap_list.front());

        {
            LOCK_MGMT();
            EXPECT_NO_THROW(api::destroyVolume(vol_id,
                                               DeleteLocalData::F,
                                               RemoveVolumeCompletely::F,
                                               DeleteVolumeNamespace::F,
                                               ForceVolumeDeletion::F));
            vol = 0;
        }

        const VolumeId clone_id(clone_ns.str());
        const OwnerTag clone_owner_tag(new_owner_tag());

        {
            LOCK_MGMT();

            const auto params = CloneVolumeConfigParameters(clone_id,
                                                            clone_ns,
                                                            ns,
                                                            clone_owner_tag)
                .parent_snapshot(snap);

            ASSERT_NO_THROW(api::createClone(params,
                                             PrefetchVolumeData::T));
        }

        Volume* clone = 0;

        {
            LOCK_MGMT();
            EXPECT_NO_THROW(clone = api::getVolumePointer(clone_id));
        }

        vol_list.clear();

        {
            LOCK_MGMT();
            EXPECT_NO_THROW(api::getVolumeList(vol_list));
        }

        found = false;
        BOOST_FOREACH(VolumeId& i, vol_list)
        {
            if (i == clone_id)
            {
                found = true;
            }
        }

        EXPECT_TRUE(found);

        writeAndCheckFirstCluster(clone, '2');

        std::string snap2;
        {
            LOCK_MGMT();
            EXPECT_NO_THROW(snap2 = api::createSnapshot(clone_id));
        }

        writeAndCheckFirstCluster(clone, '3');

        {
            LOCK_MGMT();
            EXPECT_NO_THROW(api::scheduleBackendSync(clone_id));
        }

        sync = false;
        do
        {
            sleep (1);
            LOCK_MGMT();
            EXPECT_NO_THROW(sync = api::isVolumeSynced(clone_id));
        }
        while (not sync);

        {
            LOCK_MGMT();
            EXPECT_NO_THROW(api::destroyVolume(clone_id,
                                               DeleteLocalData::T,
                                               RemoveVolumeCompletely::F,
                                               DeleteVolumeNamespace::F,
                                               ForceVolumeDeletion::F));
            clone = 0;
        }

        {
            LOCK_MGMT();
            ASSERT_NO_THROW(api::backend_restart(clone_ns,
                                                 clone_owner_tag,
                                                 PrefetchVolumeData::F,
                                                 IgnoreFOCIfUnreachable::T,
                                                 1024));
        }

        {
            LOCK_MGMT();
            ASSERT_NO_THROW(clone = api::getVolumePointer(clone_id));
        }

        checkFirstCluster(clone, '3');

        {
            LOCK_MGMT();
            EXPECT_NO_THROW(api::restoreSnapshot(clone_id, snap2));
        }

        checkFirstCluster(clone, '2');

        {
            LOCK_MGMT();
            EXPECT_NO_THROW(api::destroyVolume(clone_id,
                                               DeleteLocalData::T,
                                               RemoveVolumeCompletely::F,
                                               DeleteVolumeNamespace::F,
                                               ForceVolumeDeletion::F));
            clone = 0;
        }

        {
            LOCK_MGMT();
            ASSERT_NO_THROW(api::local_restart(ns,
                                               parent_owner_tag,
                                               FallBackToBackendRestart::F,
                                               IgnoreFOCIfUnreachable::T,
                                               1024));
        }

        {
            LOCK_MGMT();
            ASSERT_NO_THROW(vol = api::getVolumePointer(vol_id));
        }

        checkFirstCluster(vol, '1');

        {
            LOCK_MGMT();
            EXPECT_NO_THROW(api::destroyVolume(vol_id,
                                               DeleteLocalData::F,
                                               RemoveVolumeCompletely::F,
                                               DeleteVolumeNamespace::F,
                                               ForceVolumeDeletion::F));
        }
    }

protected:
    DECLARE_LOGGER("ApiTest");
};

TEST_P(ApiTest, QueueCount)
{
    auto nspaceptr = make_random_namespace();
    const Namespace& nspace = nspaceptr->ns();

    Volume* v = newVolume("volume1",
                          nspace);

    {
        SCOPED_BLOCK_BACKEND(v);
        for(int i = 0; i < 5; i++)
        {
            writeToVolume(v,
                          0,
                          4096,
                          "xyz");
        }

        PerformanceCounters c;

        {
            fungi::ScopedLock l(api::getManagementMutex());
            EXPECT_EQ(0,
                      api::getQueueCount(VolumeId("volume1")));
            EXPECT_EQ(0,
                      api::getQueueSize(VolumeId("volume1")));
            EXPECT_EQ(0,
                      api::performance_counters(VolumeId("volume1")).backend_write_request_size.sum());
        }

        v->createSnapshot("snap1");
        {
            fungi::ScopedLock l(api::getManagementMutex());
            EXPECT_EQ(1, api::getQueueCount(VolumeId("volume1")));
            EXPECT_EQ(v->get_config().getSCOSize(),
                      api::getQueueSize(VolumeId("volume1")));
            EXPECT_EQ(0,
                      api::performance_counters(VolumeId("volume1")).backend_write_request_size.sum());
        }
    }
    while(not v->isSyncedToBackend())
    {
        sleep(1);
    }
    {
        fungi::ScopedLock l(api::getManagementMutex());
        EXPECT_EQ(0,
                  api::getQueueCount(VolumeId("volume1")));
        EXPECT_EQ(0,
                  api::getQueueSize(VolumeId("volume1")));
        EXPECT_EQ(20480,
                  api::performance_counters(VolumeId("volume1")).backend_write_request_size.sum());
        EXPECT_EQ(20480,
                  api::getStored(VolumeId("volume1")));
    }
};

TEST_P(ApiTest, SCOCacheMountPoints)
{
    const std::string mp1(directory_.string() + "/mp1");
    const std::string mp2(directory_.string() + "/mp2");
    const std::string mp3(directory_.string() + "/mp3");

    fs::remove(mp1);
    fs::remove(mp2);
    fs::remove(mp3);

    const std::string dummy(directory_.string() + "dummy");

    uint64_t fs_size = FileUtils::filesystem_size(directory_);

    EXPECT_THROW(api::addSCOCacheMountPoint(mp1,
                                            std::numeric_limits<uint64_t>::max()),
                 std::exception) <<
        "scocache mountpoint directory must exist";

    fs::create_directories(mp1);
    fs::create_directories(mp2);
    fs::create_directories(mp3);

    EXPECT_THROW(api::addSCOCacheMountPoint(mp1, fs_size * 2),
                 fungi::IOException) <<
        "specified size must not exceed filesystem size";

    EXPECT_NO_THROW(api::addSCOCacheMountPoint(mp1,
                                               std::numeric_limits<uint64_t>::max()));
    EXPECT_NO_THROW(api::addSCOCacheMountPoint(mp2,
                                               std::numeric_limits<uint64_t>::max()));
    EXPECT_NO_THROW(api::addSCOCacheMountPoint(mp3,
                                               std::numeric_limits<uint64_t>::max()));

    EXPECT_THROW(api::addSCOCacheMountPoint(mp1 + "/", 1 << 20),
                 fungi::IOException) <<
        "same mountpoint specified in different flavour must not be accepted";

    EXPECT_THROW(api::removeSCOCacheMountPoint(dummy),
                 fungi::IOException) <<
        "removing an inexistent mountpoint must fail";

    EXPECT_NO_THROW(api::removeSCOCacheMountPoint(mp3));
    EXPECT_NO_THROW(api::removeSCOCacheMountPoint(mp2));
    EXPECT_NO_THROW(api::removeSCOCacheMountPoint(mp1));

    // TODO: try removing the last mountpoint (created in VolManagerTestSetup)
}

TEST_P(ApiTest, SyncSettings)
{
    const std::string volname("volume");
    auto nsptr = make_random_namespace();

    const backend::Namespace& ns = nsptr->ns();

    const VolumeId vol_id(volname);

    newVolume(volname,
              ns);

    uint64_t number_of_syncs_to_ignore;
    uint64_t maximum_time_to_ignore_syncs_in_seconds;

    {
        fungi::ScopedLock l(api::getManagementMutex());

        ASSERT_NO_THROW(api::getSyncIgnore(vol_id,
                                           number_of_syncs_to_ignore,
                                           maximum_time_to_ignore_syncs_in_seconds));

    }

    EXPECT_EQ(number_of_syncs_to_ignore, 0);
    EXPECT_EQ(maximum_time_to_ignore_syncs_in_seconds, 0);

    const uint64_t number_of_syncs_to_ignore_c = 23;
    const uint64_t maximum_time_to_ignore_syncs_in_seconds_c = 127;

    {
        fungi::ScopedLock l(api::getManagementMutex());


        ASSERT_NO_THROW(api::setSyncIgnore(vol_id,
                                           number_of_syncs_to_ignore_c,
                                           maximum_time_to_ignore_syncs_in_seconds_c));
    }

    {
        fungi::ScopedLock l(api::getManagementMutex());


        ASSERT_NO_THROW(api::getSyncIgnore(vol_id,
                                           number_of_syncs_to_ignore,
                                           maximum_time_to_ignore_syncs_in_seconds));
    }

    EXPECT_EQ(number_of_syncs_to_ignore,
              number_of_syncs_to_ignore_c);
    EXPECT_EQ(maximum_time_to_ignore_syncs_in_seconds,
              maximum_time_to_ignore_syncs_in_seconds_c);

}

TEST_P(ApiTest, failOverCacheConfig)
{
    const std::string volname("volume");
    auto nsptr = make_random_namespace();

    const backend::Namespace& ns = nsptr->ns();


    // createTestNamespace(ns);
    const VolumeId vol_id(volname);

    Volume* v = newVolume(volname,
                          ns);
    ASSERT_TRUE(v);

    {
        fungi::ScopedLock l(api::getManagementMutex());
        const boost::optional<FailOverCacheConfig>& foc_cfg =
            api::getFailOverCacheConfig(vol_id);

        EXPECT_EQ(boost::none,
                  foc_cfg);
    }

    auto foc_ctx(start_one_foc());

    {
        fungi::ScopedLock l(api::getManagementMutex());
        api::setFailOverCacheConfig(vol_id,
                                    foc_ctx->config());

        const boost::optional<FailOverCacheConfig>& foc_cfg =
            api::getFailOverCacheConfig(vol_id);

        ASSERT_NE(boost::none,
                  foc_cfg);

        EXPECT_EQ(foc_ctx->config(),
                  *foc_cfg);
    }
}

TEST_P(ApiTest, concurrentCalls)
{
    boost::condition_variable cond;
    boost::mutex mutex;
    const Namespace nspace;
    const Namespace clone_ns;
    const Namespace nspace2;
    const Namespace nspace3;
    const VolumeId vol_id(nspace.str());
    std::string snap;
    std::string snap2;
    std::string snap3;

    testApi(
            // nspace,
            // clone_ns,
            snap);

    bool run = false;

    boost::thread t1(boost::bind(&ApiTest::fakeRestart,
                                 this,
                                 clone_ns,
                                 &mutex,
                                 &cond,
                                 &run));
    {
        boost::unique_lock<boost::mutex> l(mutex);
        while (not run)
        {
            cond.wait(l);
        }
    }

    boost::thread t2(boost::bind(&ApiTest::testApi,
                                 this,
                                 // nspace2,
                                 // backend::Namespace(),
                                 snap2));

    boost::thread t3(boost::bind(&ApiTest::testApi,
                                 this,
                                 // nspace3,
                                 // backend::Namespace(),
                                 snap3));

    {
        LOCK_MGMT();
        const VolumeId vol_id(clone_ns.str());
        EXPECT_THROW(api::getVolumePointer(vol_id),
                     std::exception);
    }

    {
        LOCK_MGMT();
        EXPECT_THROW(api::local_restart(clone_ns,
                                        new_owner_tag(),
                                        FallBackToBackendRestart::F,
                                        IgnoreFOCIfUnreachable::T,
                                        1024),
                     std::exception);
    }

    {
        LOCK_MGMT();
        EXPECT_THROW(api::backend_restart(clone_ns,
                                          new_owner_tag(),
                                          PrefetchVolumeData::F,
                                          IgnoreFOCIfUnreachable::T,
                                          1024),
                     std::exception);
    }

    {
        LOCK_MGMT();
        const VolumeSize vol_size(1 << 30);
        EXPECT_THROW(api::createNewVolume(VanillaVolumeConfigParameters(vol_id,
                                                                        clone_ns,
                                                                        vol_size,
                                                                        new_owner_tag())),
                     std::exception);
    }

    {
        LOCK_MGMT();

        const auto params = CloneVolumeConfigParameters(VolumeId(clone_ns.str()),
                                                        clone_ns,
                                                        nspace,
                                                        new_owner_tag())
            .parent_snapshot(snap);

        EXPECT_THROW(api::createClone(params,
                                      PrefetchVolumeData::F),
                     std::exception);
    }

    // add more api calls here - they should all fail as the volume is
    // invisible while restarting

    std::list<VolumeId> vol_list;
    {
        LOCK_MGMT();
        api::getVolumeList(vol_list);
    }

    bool found = false;
    BOOST_FOREACH(VolumeId& i, vol_list)
    {
        if (i == vol_id)
        {
            found = true;
        }
    }

    EXPECT_FALSE(found);

    t2.join();
    t3.join();

    {
        boost::unique_lock<boost::mutex> l(mutex);
        run = false;
    }

    cond.notify_one();
    t1.join();
}

TEST_P(ApiTest, MetaDataStoreMaxPages)
{
    LOCK_MGMT();

    const VolumeId volid("volume");

    const VolumeSize volsize(1ULL << 40);
    const uint32_t max_pages = 1365;

    auto nsidptr = make_random_namespace();

    const Namespace& nsid = nsidptr->ns();

    const OwnerTag otag(new_owner_tag());

    auto params = VanillaVolumeConfigParameters(volid,
                                                nsid,
                                                volsize,
                                                otag)
        .metadata_cache_pages(max_pages);

    api::createNewVolume(params);

    {
        const MetaDataStoreStats mds = api::getMetaDataStoreStats(volid);
        EXPECT_EQ(max_pages, mds.max_pages);
        EXPECT_EQ(0, mds.cached_pages);
    }

    api::destroyVolume(volid,
                       DeleteLocalData::F,
                       RemoveVolumeCompletely::F,
                       DeleteVolumeNamespace::F,
                       ForceVolumeDeletion::F);
    api::local_restart(nsid,
                       otag,
                       FallBackToBackendRestart::F,
                       IgnoreFOCIfUnreachable::T,
                       max_pages);

    {
        const MetaDataStoreStats mds = api::getMetaDataStoreStats(volid);
        EXPECT_EQ(max_pages, mds.max_pages);
        EXPECT_EQ(0, mds.cached_pages);
    }

    api::destroyVolume(volid,
                       DeleteLocalData::T,
                       RemoveVolumeCompletely::F,
                       DeleteVolumeNamespace::F,
                       ForceVolumeDeletion::F);
    api::backend_restart(nsid,
                         new_owner_tag(),
                         PrefetchVolumeData::F,
                         IgnoreFOCIfUnreachable::T,
                         max_pages);

    {
        const MetaDataStoreStats mds = api::getMetaDataStoreStats(volid);
        EXPECT_EQ(max_pages, mds.max_pages);
        EXPECT_EQ(0, mds.cached_pages);
    }
}

TODO("Y42 Enhance this test to check failovercache");

TEST_P(ApiTest, destroyVolumeVariants)
{
    const VolumeId volid("volume");

    const VolumeSize volsize(1ULL << 40);

    const Namespace nsid;
    const OwnerTag otag(new_owner_tag());

    LOCK_MGMT();
    {
        auto nsidptr = make_random_namespace(nsid);

        ASSERT_NO_THROW(api::createNewVolume(VanillaVolumeConfigParameters(volid,
                                                                           nsid,
                                                                           volsize,
                                                                           otag)));

        ASSERT_NO_THROW(api::destroyVolume(volid,
                                           DeleteLocalData::F,
                                           RemoveVolumeCompletely::F,
                                           DeleteVolumeNamespace::F,
                                           ForceVolumeDeletion::F));

        ASSERT_THROW(api::backend_restart(nsid,
                                          otag,
                                          PrefetchVolumeData::F,
                                          IgnoreFOCIfUnreachable::T,
                                          1024),
                     fungi::IOException);


        ASSERT_NO_THROW(api::local_restart(nsid,
                                           otag,
                                           FallBackToBackendRestart::F,
                                           IgnoreFOCIfUnreachable::T,
                                           1024));


        ASSERT_NO_THROW(api::destroyVolume(volid,
                                           DeleteLocalData::T,
                                           RemoveVolumeCompletely::F,
                                           DeleteVolumeNamespace::F,
                                           ForceVolumeDeletion::F));

        ASSERT_THROW(api::local_restart(nsid,
                                        otag,
                                        FallBackToBackendRestart::F,
                                        IgnoreFOCIfUnreachable::T,
                                        1024),
                     fungi::IOException);

        ASSERT_NO_THROW(api::backend_restart(nsid,
                                             otag,
                                             PrefetchVolumeData::F,
                                             IgnoreFOCIfUnreachable::T,
                                             1024));

        auto foc_ctx(start_one_foc());

        setFailOverCacheConfig(volid,
                               foc_ctx->config());

        const fs::path foc_ns_path = foc_ctx->path() / nsid.str();

        ASSERT_NO_THROW(api::destroyVolume(volid,
                                           DeleteLocalData::T,
                                           RemoveVolumeCompletely::T,
                                           DeleteVolumeNamespace::F,
                                           ForceVolumeDeletion::F));

        ASSERT_FALSE(exists(foc_ns_path));
    }

    ASSERT_THROW(api::local_restart(nsid,
                                    otag,
                                    FallBackToBackendRestart::F,
                                    IgnoreFOCIfUnreachable::T,
                                    1024),
                 fungi::IOException);


    ASSERT_THROW(api::backend_restart(nsid,
                                      otag,
                                      PrefetchVolumeData::F,
                                      IgnoreFOCIfUnreachable::T,
                                      1024),
                 fungi::IOException);
}

TEST_P(ApiTest, snapshot_metadata)
{
    LOCK_MGMT();

    const VolumeId volid("volume");

    const VolumeSize volsize(1ULL << 40);

    auto nsidptr = make_random_namespace();
    const Namespace nsid = nsidptr->ns();
    const OwnerTag otag(new_owner_tag());

    api::createNewVolume(VanillaVolumeConfigParameters(volid,
                                                       nsid,
                                                       volsize,
                                                       otag));

    Volume* vol = api::getVolumePointer(volid);

    const std::string sname1("snapshot");
    const SnapshotMetaData meta1;
    api::createSnapshot(volid, meta1, &sname1);

    waitForThisBackendWrite(vol);

    {
        const Snapshot snap(api::getSnapshot(volid, sname1));
        EXPECT_TRUE(snap.metadata().empty());
        EXPECT_TRUE(meta1 == snap.metadata());
        EXPECT_EQ(sname1, snap.getName());
    }

    const std::string sname2("snopshat");
    const SnapshotMetaData meta2(sname2.begin(), sname2.end());
    api::createSnapshot(volid, meta2, &sname2);

    waitForThisBackendWrite(vol);

    {
        const Snapshot snap(api::getSnapshot(volid, sname2));
        EXPECT_FALSE(snap.metadata().empty());
        EXPECT_TRUE(meta2 == snap.metadata());
        EXPECT_EQ(sname2, snap.getName());
    }

    {
        const std::string sname3("tanshops");
        const SnapshotMetaData meta3(SnapshotPersistor::max_snapshot_metadata_size + 1,
                                     'x');
        EXPECT_THROW(api::createSnapshot(volid, meta3, &sname3),
                     std::exception);
    }

    api::destroyVolume(volid,
                       DeleteLocalData::F,
                       RemoveVolumeCompletely::F,
                       DeleteVolumeNamespace::F,
                       ForceVolumeDeletion::F);

    api::local_restart(nsid,
                       otag,
                       FallBackToBackendRestart::F,
                       IgnoreFOCIfUnreachable::T,
                       1024);

    const Snapshot snap1(api::getSnapshot(volid, sname1));
    EXPECT_TRUE(snap1.metadata().empty());
    EXPECT_TRUE(meta1 == snap1.metadata());
    EXPECT_EQ(sname1, snap1.getName());

    const Snapshot snap2(api::getSnapshot(volid, sname2));
    EXPECT_FALSE(snap2.metadata().empty());
    EXPECT_TRUE(meta2 == snap2.metadata());
    EXPECT_EQ(sname2, snap2.getName());
}

TEST_P(ApiTest, remove_local_data)
{
    const VolumeId vname("volume");

    const VolumeSize vsize(1 << 20);

    auto nspaceptr = make_random_namespace();

    const backend::Namespace nspace = nspaceptr->ns();

    LOCK_MGMT();

    api::createNewVolume(VanillaVolumeConfigParameters(vname,
                                                       nspace,
                                                       vsize,
                                                       new_owner_tag()));

    EXPECT_TRUE(fs::exists(VolManager::get()->getMetaDataPath(nspace)));
    EXPECT_TRUE(fs::exists(VolManager::get()->getTLogPath(nspace)));

    EXPECT_THROW(api::removeLocalVolumeData(nspace),
                 fungi::IOException);

    EXPECT_TRUE(fs::exists(VolManager::get()->getMetaDataPath(nspace)));
    EXPECT_TRUE(fs::exists(VolManager::get()->getTLogPath(nspace)));

    api::destroyVolume(vname,
                       DeleteLocalData::F,
                       RemoveVolumeCompletely::F,
                       DeleteVolumeNamespace::F,
                       ForceVolumeDeletion::F);

    EXPECT_TRUE(fs::exists(VolManager::get()->getMetaDataPath(nspace)));
    EXPECT_TRUE(fs::exists(VolManager::get()->getTLogPath(nspace)));

    api::removeLocalVolumeData(nspace);

    EXPECT_FALSE(fs::exists(VolManager::get()->getMetaDataPath(nspace)));
    EXPECT_FALSE(fs::exists(VolManager::get()->getTLogPath(nspace)));

    api::removeLocalVolumeData(nspace);
}

TEST_P(ApiTest, volume_destruction_vs_monitoring)
{
    const VolumeId vname("volume");

    const VolumeSize vsize(1 << 20);
    Volume* v;

    auto nspace_ptr = make_random_namespace();
    const backend::Namespace nspace = nspace_ptr->ns();

    {
        LOCK_MGMT();

        api::createNewVolume(VanillaVolumeConfigParameters(vname,
                                                           nspace,
                                                           vsize,
                                                           new_owner_tag()));

        EXPECT_NO_THROW(api::getMetaDataStoreStats(vname));

        v = api::getVolumePointer(vname);
    }

    const unsigned wait_secs = 60;
    std::thread t;

    // Idea: block the backend and kick off volume destruction in a thread ->
    // volume destruction will get stuck trying to empty the backend queue. At the
    // same time the volume should not be visible to monitoring anymore though to
    // prevent concurrent accesses like the one in OVS-848.
    {
        SCOPED_BLOCK_BACKEND(v);

        t = std::thread([&vname]
                        {
                            LOCK_MGMT();

                            api::destroyVolume(vname,
                                               DeleteLocalData::T,
                                               RemoveVolumeCompletely::T,
                                               DeleteVolumeNamespace::F,
                                               ForceVolumeDeletion::F);
                        });

        bool gone = false;

        for (unsigned i = 0; i < (wait_secs * 1000 / 2); ++i)
        {
            std::list<VolumeId> l;

            {
                LOCK_MGMT();
                api::getVolumeList(l);
            }

            if (not l.empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            else
            {
                gone = true;
                break;
            }
        }

        ASSERT_TRUE(gone);

        LOCK_MGMT();

        EXPECT_THROW(api::getVolumePointer(vname),
                     VolManager::VolumeDoesNotExistException);

        EXPECT_THROW(api::getMetaDataStoreStats(vname),
                     VolManager::VolumeDoesNotExistException);
    }

    t.join();
}

TEST_P(ApiTest, snapshot_restoration)
{
    const VolumeId volid("volume");

    const VolumeSize vsize(1 << 20);
    Volume* vol;

    auto nspace_ptr = make_random_namespace();
    const backend::Namespace nspace = nspace_ptr->ns();

    {
        LOCK_MGMT();

        api::createNewVolume(VanillaVolumeConfigParameters(volid,
                                                           nspace,
                                                           vsize,
                                                           new_owner_tag()));
        vol = api::getVolumePointer(volid);
    }

    const std::string snapid("snapshot");

    {
        LOCK_MGMT();
        EXPECT_THROW(api::restoreSnapshot(volid, snapid),
                     std::exception);
    }

    const std::string before("before snapshot");

    writeToVolume(vol, before);

    {
        LOCK_MGMT();
        api::createSnapshot(volid,
                            SnapshotMetaData(),
                            &snapid);
    }

    const std::string after("after snapshot");

    writeToVolume(vol, after);

    waitForThisBackendWrite(vol);

    checkVolume(vol, after);

    {
        LOCK_MGMT();
        EXPECT_THROW(api::restoreSnapshot(volid, "no-such-snapshot"),
                     std::exception);
    }

    checkVolume(vol, after);

    {
        LOCK_MGMT();
        api::restoreSnapshot(volid, snapid);
    }

    checkVolume(vol, before);
}

INSTANTIATE_TEST(ApiTest);

}

// Local Variables: **
// mode: c++ **
// End: **
