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

#include "../VolumeConfig.h"
#include "../Api.h"
#include "../FailOverCacheAsyncBridge.h"
#include "../FailOverCacheSyncBridge.h"

#include <stdlib.h>

#include <future>

namespace volumedriver
{

class FailOverCacheTester
    : public VolManagerTestSetup
{
public:
    FailOverCacheTester()
        : VolManagerTestSetup("FailOverCacheTester")
    {}

    template<typename B>
    uint64_t
    get_num_entries(B& bridge)
    {
        // Otherwise we might clash with the FailOverCacheBridge's ping mechanism!
        boost::lock_guard<decltype(bridge.mutex_)> g(bridge.mutex_);
        size_t num_clusters = 0;
        bridge.cache_->getEntries([&](ClusterLocation,
                                      uint64_t /* lba */,
                                      const byte* /* buf */,
                                      size_t /* size */)
                                  {
                                      ++num_clusters;
                                  });

        return num_clusters;
    }

    void
    check_num_entries(Volume& v,
                      uint64_t expected)
    {
        FailOverCacheClientInterface* bridge = getFailOverWriter(&v);
        uint64_t entries = 0;

        switch (bridge->mode())
        {
        case FailOverCacheMode::Asynchronous:
            entries = get_num_entries(dynamic_cast<FailOverCacheAsyncBridge&>(*bridge));
            break;
        case FailOverCacheMode::Synchronous:
            entries = get_num_entries(dynamic_cast<FailOverCacheSyncBridge&>(*bridge));
            break;
        }

        EXPECT_EQ(expected,
                  entries);
    }
};

TEST_P(FailOverCacheTester, empty_flush)
{
    auto foc_ctx(start_one_foc());
    VolumeId vid("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(vid,
                          ns);
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));
    v->sync();
}

TEST_P(FailOverCacheTester, focTimeout)
{
    auto foc_ctx(start_one_foc());
    VolumeId vid("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(vid,
                          ns);
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    {
        fungi::ScopedLock l(api::getManagementMutex());
        api::setFOCTimeout(vid,
                           boost::chrono::seconds(100));
    }

     destroyVolume(v,
                   DeleteLocalData::T,
                   RemoveVolumeCompletely::T);

}

TEST_P(FailOverCacheTester, VolumeWithFOC)
{
    auto foc_ctx(start_one_foc());

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    for(int i =0; i < 128; ++i)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "xyz");
    }

    checkVolume(v,
                0,
                4096,
                "xyz");

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, VolumeWithoutFOC)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);

    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                               5610,
                                                               GetParam().foc_mode())),
                 fungi::IOException);


    for(int i =0; i < 128; ++i)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "xyz");
    }
    checkVolume(v,
                0,
                4096,
                "xyz");

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, StopRace)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = 0;

    {
        auto foc_ctx(start_one_foc());
        ASSERT_FALSE(v);
        v = newVolume("vol1",
                      ns);
        ASSERT_TRUE(v);

        v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

        for(int i =0; i < 128; ++i)
        {

            writeToVolume(v,
                          0,
                          4096,
                          "xyz");
        }
    }

    for(int i =0; i < 4; ++i)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "xyz");
    }

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, StopCacheServer)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = 0;

    {
        auto foc_ctx(start_one_foc());

        ASSERT_FALSE(v);

        v = newVolume("vol1",
                      ns);

        ASSERT_TRUE(v);

        v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

        for(int i = 0; i < 128; ++i)
        {
            writeToVolume(v,
                          0,
                          4096,
                          "xyz");
        }
    }

    for(int i =0; i < 128; ++i)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "xyz");
    }

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, CacheServerHasNoMemory)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = 0;
    int numClusters = 128;

    {
        auto foc_ctx(start_one_foc());
        ASSERT_FALSE(v);

        v = newVolume("vol1",
                       ns);
        ASSERT_TRUE(v);

        v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

        ASSERT_EQ(VolumeFailOverState::OK_SYNC,
                  v->getVolumeFailOverState());

        for (int i = 0; i < numClusters; ++i)
        {
            writeToVolume(v,
                          0,
                          4096,
                          "xyz");
        }

        flushFailOverCache(v);
        check_num_entries(*v, numClusters);
    }

    flushFailOverCache(v); // It is only detected for sure that the FOC is gone when something happens over the wire

    ASSERT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    {
        auto foc_ctx(start_one_foc());

        v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

        check_num_entries(*v, 0);
        destroyVolume(v,
                      DeleteLocalData::T,
                      RemoveVolumeCompletely::T);
    }
}

TEST_P(FailOverCacheTester, ResetCacheServer)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);
    uint16_t port = 2999;
    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                               port,
                                                               GetParam().foc_mode())),
                  fungi::IOException);

    auto foc_ctx(start_one_foc());
    for(int i = 0; i < 4; ++i)
    {
        if(i % 2)
        {
            auto foc_ctx(start_one_foc());
            v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));
        }

        writeToVolume(v,
                      0,
                      4096,
                      "xyz");
    }
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, ClearCacheServer)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
			  ns);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    const unsigned numwrites = drand48() * 128;

    for(unsigned i = 0; i < numwrites; ++i)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "xyz");
    }

    getFailOverWriter(v)->Flush();

    check_num_entries(*v, numwrites);

    getFailOverWriter(v)->Clear();

    check_num_entries(*v, 0);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, test2)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

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
    Volume* v2 = 0;
    ASSERT_NO_THROW(v2 = localRestart(ns));

    checkVolume(v2,
                0,
                4096,
                "xyz");

    destroyVolume(v2,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, test3)
{
    auto foc_ctx(start_one_foc());

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    for(int i =0; i < 50; ++i)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "xyz");
    }

    checkVolume(v,
                0,
                4096,
                "xyz");

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, resetToSelf)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    const TLogId tlog_id1(v->getSnapshotManagement().getCurrentTLogId());
    const unsigned entries = 32;

    for(unsigned i = 0; i < entries; i++)
    {
        writeToVolume(v, i * 4096, 4096, "bdv");
    }
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    const TLogId tlog_id2(v->getSnapshotManagement().getCurrentTLogId());
    EXPECT_NE(tlog_id1,
              tlog_id2);

    for(unsigned i = 0; i < entries; i++)
    {
        writeToVolume(v, i * 4096, 4096, "bdv");
    }

    flushFailOverCache(v);

    check_num_entries(*v,
                      entries);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, resetToOther)
{
    auto foc_ctx1(start_one_foc());
    auto foc_ctx2(start_one_foc());

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);

    const unsigned entries = 32;

    for(unsigned i = 0; i < entries; i++)
    {
        writeToVolume(v, i * 4096, 4096, "bdv");
    }
    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());

    EXPECT_NO_THROW(v->setFailOverCacheConfig(foc_ctx1->config(GetParam().foc_mode())));

    EXPECT_NO_THROW(v->setFailOverCacheConfig(foc_ctx2->config(GetParam().foc_mode())));

    EXPECT_NO_THROW(v->setFailOverCacheConfig(foc_ctx1->config(GetParam().foc_mode())));

    EXPECT_TRUE(v->getVolumeFailOverState() == VolumeFailOverState::KETCHUP or
                v->getVolumeFailOverState() == VolumeFailOverState::OK_SYNC);

    for(unsigned i = 0; i < 10; ++i)
    {
        if(v->getVolumeFailOverState() == VolumeFailOverState::OK_SYNC)
            break;
        sleep(1);
    }

    EXPECT_EQ(VolumeFailOverState::OK_SYNC,
              v->getVolumeFailOverState());

    for(unsigned i = 0; i < entries; i++)
    {
        writeToVolume(v, i * 4096, 4096, "bdv");
    }
}

TEST_P(FailOverCacheTester, AutoRecoveries)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);

    const auto port = get_next_foc_port();
    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                               port,
                                                               GetParam().foc_mode())),
                 fungi::IOException);

    ASSERT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    auto foc_ctx(start_one_foc());
    ASSERT_EQ(port,
              foc_ctx->port());

    sleep(2 * failovercache_check_interval_in_seconds_);
    ASSERT_EQ(VolumeFailOverState::OK_SYNC,
              v->getVolumeFailOverState());
}

TEST_P(FailOverCacheTester, TLogsAreRemoved)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    for(int i =0; i < 16; ++i)
    {
        std::stringstream ss;
        ss << i;
        for(int j = 0; j < 32; ++j)
        {
            writeToVolume(v, j*4096,4096, "bdv");
        }
        waitForThisBackendWrite(v);
        createSnapshot(v,ss.str());
    }

    for(int j = 0; j < 32; ++j)
    {
        writeToVolume(v, j*4096,4096, "bdv");
    }

    waitForThisBackendWrite(v);

    std::list<std::string> files;
    getFailOverWriter(v)->Flush();

    fs::directory_iterator it(foc_ctx->path());
    fs::directory_iterator end;

    auto fun([&]() -> fs::path
             {
                 // we have the lockfile and the actual data.
                 boost::optional<fs::path> nspace_dir;
                 unsigned count = 0;
                 for (fs::directory_iterator it(foc_ctx->path()); it != end; ++it)
                 {
                     ++count;
                     if (it->path() != foc_ctx->lock_file())
                     {
                         EXPECT_EQ(boost::none,
                                   nspace_dir);

                         nspace_dir = it->path();
                     }
                 }

                 EXPECT_NE(boost::none,
                           nspace_dir);
                 EXPECT_EQ(2U,
                           count);

                 return *nspace_dir;
             });

    const fs::path nspace_dir(fun());
    EXPECT_FALSE(fs::is_empty(nspace_dir));

    createSnapshot(v,"boomboom");
    waitForThisBackendWrite(v);

    getFailOverWriter(v)->Flush();

    EXPECT_EQ(nspace_dir,
              fun());

    EXPECT_TRUE(fs::is_empty(nspace_dir));

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, DirectoryRemovedOnUnRegister)
{
    auto foc_ctx(start_one_foc());

    ASSERT_TRUE(fs::exists(foc_ctx->path()));
    ASSERT_TRUE(fs::is_directory(foc_ctx->path()));
    ASSERT_FALSE(fs::is_empty(foc_ctx->path()));

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    const fs::path foc_ns_path(foc_ctx->path() / ns.str());

    ASSERT_TRUE(exists(foc_ns_path));
    ASSERT_TRUE(fs::is_directory(foc_ns_path));
    ASSERT_TRUE(fs::is_empty(foc_ns_path));

    writeToVolume(v, 0, 4096, "bdv");
    v->sync();

    ASSERT_FALSE(fs::is_empty(foc_ns_path));

    v->setFailOverCacheConfig(boost::none);

    ASSERT_TRUE(fs::exists(foc_ctx->path()));
    ASSERT_TRUE(fs::is_directory(foc_ctx->path()));

    ASSERT_FALSE(fs::exists(foc_ns_path));
    ASSERT_FALSE(fs::is_directory(foc_ns_path));
}

// OVS-3430: a nasty race between flush and addEntries for the newData buffer
TEST_P(FailOverCacheTester, adding_and_flushing)
{
    auto foc_ctx(start_one_foc());
    auto wrns(make_random_namespace());

    Volume* v = newVolume(*wrns);
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    FailOverCacheClientInterface& foc = *v->getFailOver();

    std::atomic<bool> stop(false);
    size_t flushes = 0;

    auto future(std::async(std::launch::async,
                           [&]
                           {
                               while (not stop)
                               {
                                   foc.Flush();
                                   ++flushes;
                                   boost::this_thread::sleep_for(boost::chrono::milliseconds(5));
                               }
                           }));

    const size_t csize = v->getClusterSize();
    std::vector<byte> buf(csize);
    std::vector<ClusterLocation> locs(1);

    const size_t max = (1ULL << 27) / buf.size();

    for (size_t i = 1; i <= max; ++i)
    {
        *reinterpret_cast<size_t*>(buf.data()) = i;
        locs[0] = ClusterLocation(i);

        while (not foc.addEntries(locs,
                                  locs.size(),
                                  i,
                                  buf.data()))
        {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(5));
        }
    }

    EXPECT_LT(0, flushes);

    stop.store(true);

    future.wait();

    foc.Flush();

    size_t count = 0;

    for (size_t i = 1; i <= max; ++i)
    {
        foc.getSCOFromFailOver(SCO(i),
                               [&](ClusterLocation loc,
                                   uint64_t lba,
                                   const uint8_t* buf,
                                   size_t bufsize)
                               {
                                   ++count;
                                   ASSERT_EQ(i, loc.sco().number());
                                   ASSERT_EQ(i, lba);
                                   ASSERT_EQ(i, *reinterpret_cast<const size_t*>(buf));
                                   ASSERT_EQ(csize, bufsize);
                               });
    }

    EXPECT_EQ(max, count);
}

// OVS-3850: FailOverCacheProxy::clear threw an exception - let's see if this is
// inherent behaviour or something else contributed.
TEST_P(FailOverCacheTester, clear)
{
    auto wrns(make_random_namespace());
    auto foc_ctx(start_one_foc());

    FailOverCacheProxy proxy(foc_ctx->config(GetParam().foc_mode()),
                             wrns->ns(),
                             VolumeConfig::default_lba_size(),
                             VolumeConfig::default_cluster_multiplier(),
                             boost::chrono::seconds(60));

    EXPECT_NO_THROW(proxy.clear());

    const size_t csize = VolumeConfig::default_cluster_size();
    const std::vector<uint8_t> buf(csize);
    std::vector<FailOverCacheEntry> entries { FailOverCacheEntry(ClusterLocation(1),
                                                                 0,
                                                                 buf.data(),
                                                                 buf.size()) };
    proxy.addEntries(std::move(entries));

    EXPECT_NO_THROW(proxy.clear());

    foc_ctx.reset();

    EXPECT_THROW(proxy.clear(),
                 std::exception);
}

TEST_P(FailOverCacheTester, non_standard_cluster_size)
{
    auto foc_ctx(start_one_foc());
    auto wrns(make_random_namespace());
    const VolumeSize vsize(10ULL << 20);
    const ClusterMultiplier cmult(default_cluster_mult() * 2);
    const ClusterSize csize(default_lba_size() *
                            cmult);

    Volume* v = newVolume(*wrns,
                          vsize,
                          default_sco_mult(),
                          default_lba_size(),
                          cmult);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    SCOPED_BLOCK_BACKEND(v);

    auto make_pattern([](size_t i) -> std::string
                      {
                          return std::string("cluster-" +
                                             boost::lexical_cast<std::string>(i));
                      });

    const size_t num_clusters = default_sco_mult() + 3;

    for (size_t i = 0; i < num_clusters; ++i)
    {
        writeToVolume(v,
                      i * cmult,
                      csize,
                      make_pattern(i));
    }

    v->getFailOver()->Flush();

    FailOverCacheProxy proxy(foc_ctx->config(GetParam().foc_mode()),
                             wrns->ns(),
                             VolumeConfig::default_lba_size(),
                             cmult,
                             boost::chrono::seconds(60));

    size_t count = 0;

    proxy.getEntries([&](ClusterLocation /* loc */,
                         uint64_t lba,
                         const uint8_t* buf,
                         size_t bufsize)
                     {
                         ASSERT_EQ(csize,
                                   bufsize);
                         ASSERT_EQ(count * cmult,
                                   lba);
                         const std::string p(make_pattern(count));

                         for (size_t i = 0; i < bufsize; ++i)
                         {
                             ASSERT_EQ(p[i % p.size()],
                                       buf[i]);
                         }

                         ++count;
                     });

    EXPECT_EQ(num_clusters,
              count);
}

// needs to be run as root, and messes with the iptables, so use with _extreme_
// caution
/*
TEST_P(FailOverCacheTester, DISABLED_tarpit)
{
    ASSERT_EQ(0, ::geteuid()) << "test needs to run with root privileges!";


    auto foc_ctx(start_one_foc());


    Volume* v = newVolume("volume",
			  "volumeNamespace");

    v->setFailOverCache(foc_ctx->config(GetParam().foc_mode()));

    VolumeConfig cfg = v->get_config();

    std::stringstream ss;

    ss << foc_ctx->port();

    const std::string rule("-p TCP --dport " + ss.str() +
                           " -j DROP");

    const std::string cmd("/sbin/iptables");
    const std::string add_cmdline(cmd + " -A INPUT " + rule);
    const std::string del_cmdline(cmd + " -D INPUT " + rule);

    ::system(add_cmdline.c_str());

    const std::string pattern = "notInFOC";

    for (size_t i = 0; i < 128; ++i) {
        writeToVolume(v,
                      0,
                      v->getClusterSize(),
                      pattern);
    }

    createSnapshot(v, "snap");
    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);

    EXPECT_EQ(VolumeInterface::VolumeState::DEGRADED, v->getVolumeFailOverState());

    Volume* c = createClone("clone",
                            "cloneNamespace",
                            "volumeNamespace",
                            "snap");

    checkVolume(c,
                0,
                c->getClusterSize(),
                pattern);

    ::system(del_cmdline.c_str());
}
*/


namespace
{

const VolumeDriverTestConfig cluster_cache_config =
    VolumeDriverTestConfig().use_cluster_cache(true);

const VolumeDriverTestConfig sync_foc_config =
    VolumeDriverTestConfig()
    .use_cluster_cache(true)
    .foc_mode(FailOverCacheMode::Synchronous);

}

INSTANTIATE_TEST_CASE_P(FailOverCacheTesters,
                        FailOverCacheTester,
                        ::testing::Values(cluster_cache_config,
                                          sync_foc_config));

}

// Local Variables: **
// mode: c++ **
// End: **
