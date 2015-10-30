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

#include "VolManagerTestSetup.h"

#include "../ArakoonMetaDataBackend.h"
#include "../CachedMetaDataStore.h"
#include "../DataStoreNG.h"
#include "../TokyoCabinetMetaDataBackend.h"
#include "../VolManager.h"
#include "../VolumeConfig.h"
#include <boost/range/adaptor/reversed.hpp>
namespace volumedriver
{

class VolManagerRestartTest
    : public VolManagerTestSetup
{
public:
    VolManagerRestartTest()
        : VolManagerTestSetup("VolManagerRestartTest")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }
};

TEST_P(VolManagerRestartTest, checkEmptyDirectoryRestart1)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = 0;
    v = newVolume("volume1",
                  ns);
    ASSERT_TRUE(v);
    VolumeConfig cfg  = v->get_config();
    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v = 0;

    v = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v);

    // Directories are now automatically cleaned!!
    ASSERT_THROW(restartVolume(cfg),
                 fungi::IOException);
}

TEST_P(VolManagerRestartTest, checkEmptyDirectoryRestart2)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();


    Volume* v = 0;
    v = newVolume("volume1",
		  ns);
    ASSERT_TRUE(v);
    VolumeConfig cfg  = v->get_config();
    v->scheduleBackendSync();
    while(not v->isSyncedToBackend())
    {
        sleep(1);
    }


    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    v = 0;

    v = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v);

    ASSERT_NO_THROW(restartVolume(cfg));
}

TEST_P(VolManagerRestartTest, localrestart1)
{
    Volume* v = 0;
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();



    ASSERT_THROW(localRestart(ns),
                 std::exception);
    v = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v);
}

TEST_P(VolManagerRestartTest, localrestart2)
{
    Volume* v = 0;
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();


        //    backend::Namespace ns;

    v = newVolume("volume1",
                  ns);
    ASSERT_TRUE(v);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v = 0;
    ASSERT_NO_THROW(v = localRestart(ns));

    ASSERT_TRUE(v);
    checkCurrentBackendSize(v);
}

TEST_P(VolManagerRestartTest, localrestart_check_readcachesetting1)
{
    Volume* v = 0;
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();


        //    backend::Namespace ns;

    v = newVolume("volume1",
		  ns);
    boost::optional<ClusterCacheBehaviour> behaviour(ClusterCacheBehaviour::NoCache);
    v->set_cluster_cache_behaviour(behaviour);

    ASSERT_TRUE(v);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v = 0;
    ASSERT_NO_THROW(v = localRestart(ns));

    ASSERT_TRUE(v);
    ASSERT_TRUE(*v->get_cluster_cache_behaviour() == behaviour);
    checkCurrentBackendSize(v);
}

TEST_P(VolManagerRestartTest, localrestart_check_readcachesetting2)
{
    Volume* v = 0;
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();


        // backend::Namespace ns;
    v = newVolume("volume1",
		  ns);
    boost::optional<ClusterCacheBehaviour> behaviour(ClusterCacheBehaviour::CacheOnWrite);
    v->set_cluster_cache_behaviour(behaviour);

    ASSERT_TRUE(v);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v = 0;
    ASSERT_NO_THROW(v = localRestart(ns));

    ASSERT_TRUE(v);
    ASSERT_TRUE(*v->get_cluster_cache_behaviour() == behaviour);
    checkCurrentBackendSize(v);
}

TEST_P(VolManagerRestartTest, localrestart_check_readcachesetting3)
{
    Volume* v = 0;
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();


        // backend::Namespace ns;
    v = newVolume("volume1",
		  ns);
    boost::optional<ClusterCacheBehaviour> behaviour(ClusterCacheBehaviour::CacheOnRead);
    v->set_cluster_cache_behaviour(behaviour);

    ASSERT_TRUE(v);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v = 0;
    ASSERT_NO_THROW(v = localRestart(ns));

    ASSERT_TRUE(v);
    ASSERT_TRUE(*v->get_cluster_cache_behaviour() == behaviour);
    checkCurrentBackendSize(v);
}

TEST_P(VolManagerRestartTest, localrestart3)
{
    Volume* v = 0;
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();


    //    backend::Namespace ns;
    v = newVolume("volume1",
                  ns);
    ASSERT_TRUE(v);
    writeToVolume(v,0,4096,"bartd");

    // Put some bogus data in the metadatastore...
    ClusterLocationAndHash loc_and_hash(ClusterLocation(5,
                                                        0,
                                                        SCOCloneID(0),
                                                        SCOVersion(1)),
                                        growWeed());

    v->getMetaDataStore()->writeCluster(8192,
                                        loc_and_hash);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    ASSERT_NO_THROW(v = localRestart(ns));
    checkVolume(v,0,4096,"bartd");
    checkCurrentBackendSize(v);
}

TEST_P(VolManagerRestartTest, test1)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("volume1",
                          ns);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, test2)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    // backend::Namespace ns;
    Volume* v = newVolume("volume1",
                          ns);
    writeToVolume(v,
                  0,
                  4096,
                  "xyz");
    checkVolume(v,
                0,
                4096,
                "xyz");

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v = 0;

    ASSERT_NO_THROW(v = localRestart(ns));
    checkVolume(v,
                0,
                4096,
                "xyz");
    checkCurrentBackendSize(v);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, testRescheduledSCOS)
{
    VolumeId v1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    // backend::Namespace ns;
    Volume* v = newVolume(v1,
                          ns);
    VolumeConfig cfg = v->get_config();
    writeToVolume(v, 0, 4096, "doh");
    ClusterLocation l(1);

    std::string scoptr_name =
        v->getDataStore()->getSCO(l.sco(), 0)->path().string();

    v->createSnapshot("first");

    waitForThisBackendWrite(v);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    // make ze cleanslate ze inlined
    VolManager* man = VolManager::get();
    fs::remove_all(man->getTLogPath(cfg));
    fs::remove_all(man->getMetaDataPath(cfg));
    // Fuck with the SCOCache in a mean and unbecoming way.
    struct stat st;
    int ret = ::stat(scoptr_name.c_str(), &st);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(st.st_mode bitand S_ISVTX);
    ret = ::chmod(scoptr_name.c_str(), st.st_mode ^ S_ISVTX);
    v = 0;
    // This will assert since it reschedules the sole sco to backend
     restartVolume(cfg);
     v = getVolume(v1);

     ASSERT_TRUE(v);
     checkCurrentBackendSize(v);
     waitForThisBackendWrite(v);
}

TEST_P(VolManagerRestartTest, test3)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    // backend::Namespace ns;
    Volume* v = newVolume("volume1",
                          ns);

    writeToVolume(v, 0, 4096, "xyz");
    checkVolume(v, 0, 4096, "xyz");

    v->createSnapshot("first");

    writeToVolume(v, 0, 4096, "abc");
    checkVolume(v, 0, 4096, "abc");

    VolumeConfig cfg = v->get_config();

    waitForThisBackendWrite(v);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    Volume* vNew = 0;
    vNew = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(vNew);

    restartVolume(cfg);

    vNew = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(vNew);

    checkVolume(vNew, 0, 4096, "xyz");
    checkCurrentBackendSize(vNew);

    destroyVolume(vNew,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, RestoreSnapWith)
{
    auto foc_ctx(start_one_foc());

    VolumeId vid("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    // backend::Namespace ns;

    Volume* v = newVolume(vid,
                          ns);
    VolumeConfig cfg = v->get_config();

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    writeToVolume(v, 0, 4096, "Immanuel");
    createSnapshot(v,"snap1");
    writeToVolume(v,0, 4096, "Bart");

    v->setFailOverCacheConfig(boost::none);
    waitForThisBackendWrite(v);

    v->restoreSnapshot("snap1");

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));
    v->scheduleBackendSync();

    while(not isVolumeSyncedToBackend(v))
    {
        sleep(1);
    }

    flushFailOverCache(v);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    v = 0;
    restartVolume(cfg);
    v = getVolume(vid);
    ASSERT_TRUE(v);

    checkVolume(v,0, 4096,"Immanuel");
    checkCurrentBackendSize(v);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, DoubleRestartSmallTLogs)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    //backend::Namespace ns;
    Volume* v = newVolume("volume1",
                          ns);

    for(int i = 0; i < 128; ++i)
    {
        writeToVolume(v,i*1024,1024,"xyz");
    }

    scheduleBackendSync(v);

    while(not isVolumeSyncedToBackend(v))
    {
        sleep(1);
    }

    VolumeConfig cfg = v->get_config();
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    v = 0;
    v = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v);

    restartVolume(cfg);
    v = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v);
    for(int i = 0; i < 128; ++i)
    {
         checkVolume(v,i*1024,1024,"xyz");
    }


    for(int i =0; i < 128; ++i)
    {

        writeToVolume(v, i*1024, 1024,"abc");
    }

    scheduleBackendSync(v);

    while(not isVolumeSyncedToBackend(v))
    {
        sleep(1);
    }

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    v=0;

    v = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v);

    restartVolume(cfg);
    v = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v);

    for(int i = 0; i < 128; ++i)
    {
        checkVolume(v,i*1024,1024,"abc");
    }
    checkCurrentBackendSize(v);
}

TEST_P(VolManagerRestartTest, DoubleRestart)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    // backend::Namespace ns;
    Volume* v = newVolume("volume1",
                          ns);

    VolumeConfig cfg = v->get_config();

    size_t size = 20 << 10;

    writeToVolume(v,0,size,"xyz");

    scheduleBackendSync(v);

    while(not isVolumeSyncedToBackend(v))
    {
        sleep(1);
    }

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    v = 0;
    v = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v);

    restartVolume(cfg);
    v = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v);

    checkVolume(v,0,size,"xyz");
    writeToVolume(v, 0, size,"abc");

    scheduleBackendSync(v);

    while(not isVolumeSyncedToBackend(v))
    {
        sleep(1);
    }

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    v=0;

    v = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v);

    restartVolume(cfg);
    v = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v);
    checkVolume(v,0,size,"abc");
    checkCurrentBackendSize(v);
}

TEST_P(VolManagerRestartTest, test4)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    // backend::Namespace ns;
    Volume* v = newVolume("volume1",
			  ns);

    size_t size = 20 << 10;
    writeToVolume(v, 0, size, "xyz");
    checkVolume(v, 0, size, "xyz");

    v->createSnapshot("first");

    writeToVolume(v, 0, size, "abc");
    checkVolume(v, 0, size, "abc");
    waitForThisBackendWrite(v);

    v->createSnapshot("second");

    writeToVolume(v, 0, size, "def");
    checkVolume(v, 0, size, "def");

    waitForThisBackendWrite(v);
    VolumeConfig cfg = v->get_config();
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    Volume* vNew = 0;
    vNew = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(vNew);

    restartVolume(cfg);

    vNew = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(vNew);

    checkVolume(vNew, 0, size, "abc");
    restoreSnapshot(vNew, "first");
    checkVolume(vNew, 0, size,"xyz");
    writeToVolume(vNew, 0, 2048, "bdwz");

    checkCurrentBackendSize(vNew);

    destroyVolume(vNew,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, clone1)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    //    backend::Namespace ns;
    Volume* v = newVolume("volume1",
                          ns);

    size_t size = 20 << 10;

    writeToVolume(v, 0, size, "xyz");
    checkVolume(v, 0, size, "xyz");

    v->createSnapshot("first");
    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* clone = createClone("clone1",
                                ns1,
                                ns,
                                "first");
    clone->createSnapshot("snap1");

    checkVolume(v, 0, size, "xyz");

    waitForThisBackendWrite(clone);
    waitForThisBackendWrite(clone);

    VolumeConfig vCfg = v->get_config();
    VolumeConfig cCfg = clone->get_config();

    destroyVolume(clone,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    Volume* vClone = 0;

    vClone = getVolume(VolumeId("clone1"));
    ASSERT_FALSE(vClone);

    restartVolume(cCfg);

    vClone = getVolume(VolumeId("clone1"));
    ASSERT_TRUE(vClone);

    checkVolume(vClone,
                0,
                 size,
                 "xyz");
     writeToVolume(vClone, 0, size, "abdr");
     checkCurrentBackendSize(vClone);

     destroyVolume(vClone,
                   DeleteLocalData::T,
                   RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, test5)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    // backend::Namespace ns;
    Volume* v = newVolume("volume1",
			  ns);

    size_t size = 20 << 10;
    writeToVolume(v, 0, size, "xyz");
    checkVolume(v, 0, size, "xyz");

    v->createSnapshot("first");

    writeToVolume(v, 0, size, "abc");
    checkVolume(v, 0, size, "abc");
    waitForThisBackendWrite(v);
    v->createSnapshot("second");\
    while(not isVolumeSyncedToBackend(v))
    {
        sleep(1);
    }

    auto ns2_ptr = make_random_namespace();
    const Namespace& ns2 = ns2_ptr->ns();

    Volume* clone = createClone("clone1",
                                ns2,
                                ns,
                                "second");

    writeToVolume(clone, 0, size, "def");
    checkVolume(clone,
                0,
                size,
                "def");

    clone->createSnapshot("firstclonesnap");
    VolumeConfig vCfg = v->get_config();
    VolumeConfig cCfg = clone->get_config();

    scheduleBackendSync(v);

    while(not isVolumeSyncedToBackend(v))
    {
        sleep(1);
    }
    scheduleBackendSync(clone);

    while(not isVolumeSyncedToBackend(clone))
    {
        sleep(1);
    }

    destroyVolume(clone,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    Volume* vNew = 0;
    Volume* vClone = 0;

    vNew = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(vNew);
    vNew = getVolume(VolumeId("clone1"));
    ASSERT_FALSE(vNew);

    restartVolume(vCfg);
    restartVolume(cCfg);

    vNew = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(vNew);
    vClone = getVolume(VolumeId("clone1"));
    ASSERT_TRUE(vClone);

    checkVolume(vNew,
                0,
                size,
                "abc");
    checkCurrentBackendSize(vNew);

    checkVolume(vClone,
                0,
                size,
                "def");
    checkCurrentBackendSize(vClone);

    ASSERT_NO_THROW(restoreSnapshot(vNew, "first"));
    //vNew->put();
    //vClone->put();

    writeToVolume(vClone, 0, size, "abdr");
    writeToVolume(vNew, 0, size, "abdr");
    destroyVolume(vClone,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);

    destroyVolume(vNew,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, test6)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    // backend::Namespace ns;
    Volume* v = newVolume("volume1",
                        ns);

    writeToVolume(v, 0, 16384, "xyz");

    checkVolume(v, 0, 16384, "xyz");

    v->createSnapshot("first");

    writeToVolume(v, 0, 16384, "abc");
    checkVolume(v, 0, 16384, "abc");
    waitForThisBackendWrite(v);
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* clone = createClone("clone1",
                                ns1,
                                ns,
                                "first");

    //    setTLogMaxEntries(clone, 3);

    v->createSnapshot("second");

    writeToVolume(clone,
                  0,
                  8192,
                  "def");
    checkVolume(clone,
                0,
                8192,
                "def");

    checkVolume(clone,
                16,
                8192,
                "zxy");
    checkCurrentBackendSize(clone);

    clone->createSnapshot("firstclonesnap");

    VolumeConfig vCfg = v->get_config();
    VolumeConfig cCfg = clone->get_config();

    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);
    waitForThisBackendWrite(clone);
    waitForThisBackendWrite(clone);
    destroyVolume(clone,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    Volume* vNew = 0;
    Volume* vClone = 0;

    vNew = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(vNew);
    vNew = getVolume(VolumeId("clone1"));
    ASSERT_FALSE(vNew);

    restartVolume(cCfg);
    restartVolume(vCfg);

    vNew = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(vNew);
    vClone = getVolume(VolumeId("clone1"));
    ASSERT_TRUE(vClone);


     checkVolume(vNew,
                 0,
                 16384,
                 "abc");
     checkCurrentBackendSize(vNew);
     checkVolume(vClone,
                 0,
                 8192,
                 "def");
     checkVolume(vClone,
                 16,
                 8192,
                 "zxy");
     checkCurrentBackendSize(vClone);

     //vNew->put();
     //vClone->put();
     destroyVolume(vClone,
                   DeleteLocalData::T,
                   RemoveVolumeCompletely::T);

     destroyVolume(vNew,
                   DeleteLocalData::T,
                   RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest,test7)
{
    const std::string volume = "volume";
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    // backend::Namespace ns;
    Volume* v = newVolume(volume,
                          ns);
    //    setTLogMaxEntries(v, 3);

    for(int i = 0; i < 50; ++i)
    {
        writeToVolume(v, 0, 8192, "g");
    }

    checkVolume(v, 0, 8192, "g");
    checkCurrentBackendSize(v);

    v->createSnapshot("first");

    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);

    std::vector<Volume*> clones;
    std::vector<std::unique_ptr<WithRandomNamespace> > nss;

    const uint32_t num_clones = 0xF;
    backend::Namespace prev_namespace = ns;

    for(unsigned  i = 0; i <= num_clones ; i++)
    {

        nss.push_back(make_random_namespace());
        std::string is = boost::lexical_cast<std::string>(i);

        Volume* clone = createClone(volume + boost::lexical_cast<std::string>(i),
                                    nss.back()->ns(),
                                    prev_namespace,
                                    "first");
        clones.push_back(clone);
        prev_namespace = nss.back()->ns();

        //setTLogMaxEntries(clone, 3);
        for(int j = 0; j < 50; ++j)
        {
            writeToVolume(clone, (i+1) * 16, 8192, is);
        }
        checkVolume(clone, (i+1)*16, 8192, is);
        checkCurrentBackendSize(clone);

        clone->createSnapshot("first");
        waitForThisBackendWrite(clone);
        waitForThisBackendWrite(clone);
    }

    VolumeConfig lastCloneConfig = clones[num_clones]->get_config();

    for(auto& clone : boost::adaptors::reverse(clones))
    {
        destroyVolume(clone,
                      DeleteLocalData::T,
                      RemoveVolumeCompletely::F);
    }

    // for(int i = 0xF; i >= 0; --i)
    // {
    //     destroyVolume(clones[i],
    //                   DeleteLocalData::T,
    //                   RemoveVolumeCompletely::F);
    // }

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    restartVolume(lastCloneConfig);

    Volume* lastClone = getVolume(VolumeId(volume + boost::lexical_cast<std::string>(num_clones)));

    for(unsigned i = 0; i <= num_clones; ++i)
    {
        std::string is = boost::lexical_cast<std::string>(i);

        checkVolume(lastClone,
                    (i+1) * 16,
                    8192,
                    is);
        checkCurrentBackendSize(lastClone);
    }

    //lastClone->put();
    destroyVolume(lastClone,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, same_volume_name_different_namespaces)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& nid1 = ns_ptr->ns();

    //    backend::Namespace nid1;

    Volume* v = newVolume(vid1, nid1);
    writeToVolume(v, 0, 4096, "xyz");
    v->createSnapshot("first");
    VolumeConfig cfg1 = v->get_config();
    waitForThisBackendWrite(v);

    auto ns2_ptr = make_random_namespace();
    const Namespace& nid2 = ns2_ptr->ns();


    //    Namespace nid2;
    ASSERT_THROW(newVolume(vid1,
                           nid2),
                 VolManager::VolumeNameAlreadyPresent);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    v = 0;

    v = newVolume(vid1,
                  nid2);
    writeToVolume(v, 0, 4096, "abc");
    v->createSnapshot("first");
    VolumeConfig cfg2 = v->get_config();
    waitForThisBackendWrite(v);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    restartVolume(cfg1);

    v = getVolume(vid1);
    checkVolume(v,0, 4096,"xyz");
    checkCurrentBackendSize(v);

    ASSERT_THROW(restartVolume(cfg2), VolManager::VolumeNameAlreadyPresent);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    restartVolume(cfg2);

    v = getVolume(vid1);
    checkVolume(v,0, 4096,"abc");

    ASSERT_THROW(restartVolume(cfg1), VolManager::VolumeNameAlreadyPresent);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

// Y42 what were we testing here
TEST_P(VolManagerRestartTest, DISABLED_differenttlogpath)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns);

    ASSERT_TRUE(v);
    writeToVolume(v, 0, 4096, "xyz");
    checkVolume(v, 0, 4096, "xyz");

    v->createSnapshot("first");

    writeToVolume(v, 0, 4096, "abc");
    checkVolume(v, 0, 4096, "abc");

    VolumeConfig cfg = v->get_config();
    const fs::path newTlogPath = getVolDir(VolumeId("volume1")) / "newTlogsPath";

    // Y42 what was happening here!!
    // snprintf(cfg.tlog_path,PATH_MAX,"%s",
    //          newTlogPath.file_string().c_str());

    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);
    checkCurrentBackendSize(v);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    Volume* vNew = 0;
    vNew = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(vNew);

    restartVolume(cfg);

    vNew = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(vNew);

    checkVolume(vNew, 0, 4096, "xyz");
    checkCurrentBackendSize(vNew);

    destroyVolume(vNew,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, restartNothing)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns);
    ASSERT_TRUE(v);
    writeToVolume(v, 0, 4096, "xyz");
    VolumeConfig cfg = v->get_config();
    waitForThisBackendWrite(v);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    Volume* vNew = 0;
    restartVolume(cfg);
    vNew = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(vNew);
    checkVolume(vNew, 0, 4096, "\0");
    destroyVolume(vNew,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, failovercache0)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns);
    ASSERT_TRUE(v);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    VolumeConfig cfg = v->get_config();
    writeToVolume(v,
                  0,
                  4096,
                  "abc");

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, failovercache1)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns);

    ASSERT_TRUE(v);
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode())));

    VolumeConfig cfg = v->get_config();
    writeToVolume(v,
                  0,
                  4096,
                  "abc");

    v->createSnapshot("snap1");
    waitForThisBackendWrite(v);
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,3,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);


        writeToVolume(v,
                      0,
                      4096,
                      "def");
        flushFailOverCache(v);
    }

    Volume* v1 = 0;
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v1);

    restartVolume(cfg);
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v1);
    checkVolume(v1,0,4096,"def");
    checkCurrentBackendSize(v1);

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, failovercache2)
{
    auto foc_ctx(start_one_foc());

    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns);
    ASSERT_TRUE(v);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    VolumeConfig cfg = v->get_config();
    writeToVolume(v,
                  4096,
                  4096,
                  "abc");

    v->createSnapshot("snap1");
    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,3,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);

        for(int i = 0; i < 32; i++)
        {
            std::stringstream ss;
            ss << i;

            writeToVolume(v,
                          i*512,
                          4096,
                          ss.str());
        }
    }

    Volume* v1 = 0;
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v1);

    restartVolume(cfg);
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v1);
    for(int i = 0; i < 32; ++i)
    {
        std::stringstream ss;
        ss << i;

        checkVolume(v1,i*512,4096,ss.str());
    }
    checkCurrentBackendSize(v1);

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, failovercache3)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns);

    ASSERT_TRUE(v);
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    VolumeConfig cfg = v->get_config();
        writeToVolume(v,
                      0,
                      4096,
                      "abc");

    v->createSnapshot("snap1");
    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,3,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);


        writeToVolume(v,
                      0,
                      32768,
                      "def");

        checkVolume(v,0,32768,"def");
    }

    Volume* v1 = 0;
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v1);

    restartVolume(cfg);
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v1);
    checkVolume(v1,0,32768,"def");
    checkCurrentBackendSize(v1);

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, failovercache4)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns);
    ASSERT_TRUE(v);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    VolumeConfig cfg = v->get_config();
        writeToVolume(v,
                      0,
                      4096,
                      "abc");

    v->createSnapshot("snap1");
    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);
    const uint32_t sz = 6;

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              3,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);
        uint64_t o = 0;

        for(unsigned i = 0; i < sz; i++)
        {
            unsigned s = 1 << (9 + i);
            std::stringstream ss;
            ss << i;
            writeToVolume(v, o, s, ss.str());
            o += s;
        }
    }

    Volume* v1 = 0;
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v1);

    restartVolume(cfg);
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v1);
    uint64_t o = 0;

    for(unsigned i = 0; i < sz; i++)
    {
        unsigned s = 1 << (9 + i);
        std::stringstream ss;
        ss << i;
        checkVolume(v1,o,s,ss.str());
        o += s;
    }
    checkCurrentBackendSize(v1);

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, failovercache5)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns);
    ASSERT_TRUE(v);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    VolumeConfig cfg = v->get_config();
    writeToVolume(v,
                  0,
                  4096,
                  "abc");

    v->createSnapshot("snap1");
    waitForThisBackendWrite(v);
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              3,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);
    }

    Volume* v1 = 0;
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v1);

    restartVolume(cfg);
    v1 = getVolume(VolumeId("volume1"));
    v1->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));
    waitForThisBackendWrite(v1);

    ASSERT_TRUE(v1);
    checkVolume(v1,0,4096,"abc");
    checkCurrentBackendSize(v1);

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

// backend restart failed b/c the tlog rollover during foc replay
// issued a volume sync, which in turn issued a foc flush
TEST_P(VolManagerRestartTest, focReplayAcrossTLogs)
{
    //uint32_t old_number_of_scos_in_tlog = SnapshotManagement::number_of_scos_in_tlog;

    //    SnapshotManagement::number_of_scos_in_tlog = 1;

    //    BOOST_SCOPE_EXIT((&old_number_of_scos_in_tlog))
    //    {
        //        SnapshotManagement::number_of_scos_in_tlog = old_number_of_scos_in_tlog;
    //    }
    //     BOOST_SCOPE_EXIT_END;

    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns);

    ASSERT_TRUE(v);
    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode())));

    VolumeConfig cfg = v->get_config();
    writeToVolume(v,
                  0,
                  4096,
                  "abc");

    v->createSnapshot("snap1");

    uint64_t wsize = v->getClusterSize() * (v->getSCOMultiplier() - 1);

    waitForThisBackendWrite(v);
    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              3,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);

        time_t duration = 30; // should be more than enough to trigger the issue
        time_t t0 = time(0);
        time_t t1;

        do
        {
            writeToVolume(v,
                          0,
                          wsize,
                          "def");

            scheduleBackendSync(v);

            writeToVolume(v,
                          wsize / v->getLBASize(),
                          wsize,
                          "ghi");
            t1 = time(0);
        }
        while (t1 - t0 <= duration);

        flushFailOverCache(v);

        checkVolume(v, 0, wsize, "def");
        checkVolume(v, wsize / v->getLBASize(), wsize, "ghi");
    }

    Volume* v1 = 0;
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v1);

    restartVolume(cfg);
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v1);
    checkVolume(v1, 0, wsize, "def");
    checkVolume(v1, wsize / v1->getLBASize(), wsize, "ghi");
    checkCurrentBackendSize(v1);

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, partialsnapshots)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns);

    ASSERT_TRUE(v);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    VolumeConfig cfg = v->get_config();
    writeToVolume(v,
                  0,
                  4096,
                  "a");

    v->createSnapshot("snap1");
    writeToVolume(v,
                  8,
                  4096,
                  "d");

    for(int i=0;i<2;i++)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "c");
    }

    waitForThisBackendWrite(v);

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              2,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);

        for(int i=0;i<2;++i)
        {
            writeToVolume(v,
                          0,
                          4096,
                          "c");
        }
        createSnapshot(v,"snap2");
        flushFailOverCache(v);
        writeSnapshotFileToBackend(v);
    }

    Volume* v1 = 0;
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v1);
    restartVolume(cfg);
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v1);
    // checkVolume(v1,0,4096,"c");
    // checkVolume(v1,8,4096,"d");

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

     Volume* v2 = 0;
     v2 = getVolume(VolumeId("volume1"));
     ASSERT_FALSE(v2);
     restartVolume(cfg);
     v2 = getVolume(VolumeId("volume1"));
     ASSERT_TRUE(v2);
     checkVolume(v2,0,4096,"c");
     checkVolume(v2,8,4096,"d");
     checkCurrentBackendSize(v2);
}

TEST_P(VolManagerRestartTest, datastoreOverwrite)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();


    Volume* v = newVolume(VolumeId("volume1"),
			  ns);
    ASSERT_TRUE(v);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    VolumeConfig cfg = v->get_config();
    for(int i=0;i<64;i++)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "abc");
    }

    v->createSnapshot("snap1");

    for(int i=0;i<32;i++)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "abc");
    }

    waitForThisBackendWrite(v);

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              2,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);

        for(int i=0;i<64;++i)
        {
            writeToVolume(v,
                          0,
                          4096,
                          "def");
        }

        flushFailOverCache(v);
    }

    Volume* v1 = 0;
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v1);
    restartVolume(cfg);
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(v1);
    checkVolume(v1,0,4096,"def");
    checkCurrentBackendSize(v1);

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolManagerRestartTest, backend_restart_non_empty_datastore)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& nspace = ns_ptr->ns();

    //const backend::Namespace nspace;
    const std::string volname("volume");

    Volume* v = newVolume(volname,
			  nspace);

    ASSERT_TRUE(v);
    VolumeConfig cfg = v->get_config();

    const std::string pattern1("before");
    writeToVolume(v,
                  0,
                  4096,
                  pattern1);

    v->createSnapshot("snap");
    waitForThisBackendWrite(v);

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,
                                              0,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);

        const std::string pattern2("after");
        writeToVolume(v,
                      0,
                      4096,
                      pattern2);

        checkVolume(v,
                    0,
                    4096,
                    pattern2);
    }

    // cfg.tlog_path
    fs::remove_all(VolManager::get()->getTLogPath() / nspace.str());
    // cfg.md.cached.path
    fs::remove_all(VolManager::get()->getMetaDataPath() / nspace.str());

    restartVolume(cfg,
                  PrefetchVolumeData::T);

    v = getVolume(VolumeId(volname));

    ASSERT_TRUE(v);

    checkVolume(v,
                0,
                4096,
                pattern1);
    checkCurrentBackendSize(v);
}

TEST_P(VolManagerRestartTest, testAllTlogEntriesAreReplayed)
{
    //test for VOLDRV-194
    const uint64_t replayClustersCached_orig = CachedMetaDataStore::replayClustersCached;

    BOOST_SCOPE_EXIT((&replayClustersCached_orig))
    {
        CachedMetaDataStore::replayClustersCached = replayClustersCached_orig;
    }
    BOOST_SCOPE_EXIT_END;

    if (metadata_backend_type() == MetaDataBackendType::Arakoon)
    {
        CachedMetaDataStore::replayClustersCached = 3000;
    }
    else
    {
        CachedMetaDataStore::replayClustersCached = 3000;
    }

    uint64_t volSize = 1 << 24;
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("volume1",
                          ns,
                          VolumeSize(volSize));

    writeToVolume(v, "xyz", 4096);
    checkVolume(v, "xyz", 4096);

    v->createSnapshot("first");

    VolumeConfig cfg = v->get_config();

    waitForThisBackendWrite(v);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    Volume* vNew = 0;
    vNew = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(vNew);

    restartVolume(cfg);

    vNew = getVolume(VolumeId("volume1"));
    ASSERT_TRUE(vNew);

    checkVolume(vNew, "xyz", 4096);
    checkCurrentBackendSize(vNew);

    destroyVolume(vNew,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

// reproduces a RemoteTest.volume_migration failure -- only when running with arakoon
TEST_P(VolManagerRestartTest, plain_backend_restart)
{
    const std::string name("volume");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    // const backend::Namespace ns;

    Volume* v = newVolume(name, ns, VolumeSize(1 << 20));

    const std::string pattern("pattern");
    writeToVolume(v, pattern, 4096);

    VolumeConfig cfg = v->get_config();

    v->scheduleBackendSync();
    waitForThisBackendWrite(v);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    restartVolume(cfg);
    v = getVolume(VolumeId(name));
    checkVolume(v, pattern, 4096);
}

INSTANTIATE_TEST(VolManagerRestartTest);

}

// Local Variables: **
// mode: c++ **
// End: **
