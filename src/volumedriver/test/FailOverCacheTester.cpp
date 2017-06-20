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

#include "../VolumeConfig.h"
#include "../Api.h"
#include "../FailOverCacheAsyncBridge.h"
#include "../FailOverCacheSyncBridge.h"
#include "../failovercache/ClientInterface.h"
#include "../failovercache/FileBackend.h"

#include <stdlib.h>

#include <future>

#include <youtils/Timer.h>

namespace volumedriver
{

namespace bc = boost::chrono;
namespace yt = youtils;

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
        FailOverCacheBridgeInterface* bridge = getFailOverWriter(v);
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

    SharedVolumePtr v = newVolume(vid,
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

    SharedVolumePtr v = newVolume(vid,
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

    SharedVolumePtr v = newVolume("vol1",
                          ns);
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    for(int i =0; i < 128; ++i)
    {
        writeToVolume(*v,
                      Lba(0),
                      4096,
                      "xyz");
    }

    checkVolume(*v,
                Lba(0),
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

    SharedVolumePtr v = newVolume("vol1",
                          ns);

    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                               5610,
                                                               GetParam().foc_mode())),
                 fungi::IOException);


    for(int i =0; i < 128; ++i)
    {
        writeToVolume(*v,
                      Lba(0),
                      4096,
                      "xyz");
    }
    checkVolume(*v,
                Lba(0),
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

    SharedVolumePtr v = 0;

    {
        auto foc_ctx(start_one_foc());
        ASSERT_FALSE(v);
        v = newVolume("vol1",
                      ns);
        ASSERT_TRUE(v != nullptr);

        v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

        for(int i =0; i < 128; ++i)
        {

            writeToVolume(*v,
                          Lba(0),
                          4096,
                          "xyz");
        }
    }

    for(int i =0; i < 4; ++i)
    {
        writeToVolume(*v,
                      Lba(0),
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

    SharedVolumePtr v = 0;

    {
        auto foc_ctx(start_one_foc());

        ASSERT_FALSE(v);

        v = newVolume("vol1",
                      ns);

        ASSERT_TRUE(v != nullptr);

        v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

        for(int i = 0; i < 128; ++i)
        {
            writeToVolume(*v,
                          Lba(0),
                          4096,
                          "xyz");
        }
    }

    for(int i =0; i < 128; ++i)
    {
        writeToVolume(*v,
                      Lba(0),
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

    SharedVolumePtr v = 0;
    size_t num_clusters = 0;

    {
        auto foc_ctx(start_one_foc());
        ASSERT_FALSE(v);

        v = newVolume("vol1",
                       ns);
        ASSERT_TRUE(v != nullptr);

        num_clusters = v->getSCOMultiplier() * v->getEffectiveTLogMultiplier() - 1;

        ASSERT_LT(0, num_clusters);

        v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

        ASSERT_EQ(VolumeFailOverState::OK_SYNC,
                  v->getVolumeFailOverState());

        for (size_t i = 0; i < num_clusters; ++i)
        {
            writeToVolume(*v,
                          Lba(0),
                          4096,
                          "xyz");
        }

         // only for the side effect of checking that there actually is one:
        getFailOverWriter(*v);

        flushFailOverCache(*v);
        check_num_entries(*v, num_clusters);
    }

    flushFailOverCache(*v); // It is only detected for sure that the FOC is gone when something happens over the wire

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

    SharedVolumePtr v = newVolume("vol1",
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

        writeToVolume(*v,
                      Lba(0),
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

    SharedVolumePtr v = newVolume("vol1",
			  ns);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    const unsigned numwrites = drand48() * 128;

    for(unsigned i = 0; i < numwrites; ++i)
    {
        writeToVolume(*v,
                      Lba(0),
                      4096,
                      "xyz");
    }

    getFailOverWriter(*v)->Flush().get();

    check_num_entries(*v, numwrites);

    getFailOverWriter(*v)->Clear();

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

    SharedVolumePtr v = newVolume("vol1",
                          ns);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    writeToVolume(*v,
                  Lba(0),
                  4096,
                  "xyz");

    checkVolume(*v,
                Lba(0),
                4096,
                "xyz");

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    SharedVolumePtr v2 = 0;
    ASSERT_NO_THROW(v2 = localRestart(ns));

    checkVolume(*v2,
                Lba(0),
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

    SharedVolumePtr v = newVolume("vol1",
                          ns);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    for(int i =0; i < 50; ++i)
    {
        writeToVolume(*v,
                      Lba(0),
                      4096,
                      "xyz");
    }

    checkVolume(*v,
                Lba(0),
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

    SharedVolumePtr v = newVolume("vol1",
                          ns);
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    const TLogId tlog_id1(v->getSnapshotManagement().getCurrentTLogId());
    const unsigned entries = 32;

    for(unsigned i = 0; i < entries; i++)
    {
        writeToVolume(*v, Lba(i * 4096), 4096, "bdv");
    }
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    const TLogId tlog_id2(v->getSnapshotManagement().getCurrentTLogId());
    EXPECT_NE(tlog_id1,
              tlog_id2);

    for(unsigned i = 0; i < entries; i++)
    {
        writeToVolume(*v, Lba(i * 4096), 4096, "bdv");
    }

    flushFailOverCache(*v);

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

    SharedVolumePtr v = newVolume("vol1",
                          ns);

    const unsigned entries = 32;

    for(unsigned i = 0; i < entries; i++)
    {
        writeToVolume(*v, Lba(i * 4096), 4096, "bdv");
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
        writeToVolume(*v, Lba(i * 4096), 4096, "bdv");
    }
}

TEST_P(FailOverCacheTester, TLogsAreRemoved)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume("vol1",
                          ns);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    for(int i =0; i < 16; ++i)
    {
        std::stringstream ss;
        ss << i;
        for(int j = 0; j < 32; ++j)
        {
            writeToVolume(*v, Lba(j*4096),4096, "bdv");
        }
        waitForThisBackendWrite(*v);
        createSnapshot(*v,ss.str());
    }

    for(int j = 0; j < 32; ++j)
    {
        writeToVolume(*v, Lba(j*4096),4096, "bdv");
    }

    waitForThisBackendWrite(*v);

    getFailOverWriter(*v)->Flush().get();

    auto empty([&]() -> bool
               {
                   size_t count = 0;
                   std::shared_ptr<failovercache::Backend> backend(foc_ctx->backend(ns_ptr->ns()));
                   backend->getEntries([&](ClusterLocation,
                                           int64_t /* addr */,
                                           const uint8_t* /* data */,
                                           int64_t /* size */)
                                       {
                                           ++count;
                                       });

                   return count == 0;
               });

    EXPECT_FALSE(empty());

    createSnapshot(*v,"boomboom");
    waitForThisBackendWrite(*v);

    getFailOverWriter(*v)->Flush().get();

    EXPECT_TRUE(empty());

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(FailOverCacheTester, unregister_removes_dtl_backend)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume("vol1",
                                  ns);

    EXPECT_TRUE(foc_ctx->backend(ns_ptr->ns()) == nullptr);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    EXPECT_TRUE(foc_ctx->backend(ns_ptr->ns()) != nullptr);

    v->setFailOverCacheConfig(boost::none);

    EXPECT_TRUE(foc_ctx->backend(ns_ptr->ns()) == nullptr);
}

TEST_P(FailOverCacheTester, DirectoryRemovedOnUnRegister)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume("vol1",
                                  ns);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    boost::optional<fs::path> foc_ns_path;

    {
        auto backend = std::dynamic_pointer_cast<failovercache::FileBackend>(foc_ctx->backend(ns_ptr->ns()));
        if (backend)
        {
            foc_ns_path = backend->root();
        }
    }

    if (foc_ns_path)
    {
        ASSERT_TRUE(fs::exists(*foc_ns_path));
        ASSERT_TRUE(fs::is_directory(*foc_ns_path));
        EXPECT_TRUE(fs::is_empty(*foc_ns_path));

        writeToVolume(*v, Lba(0), 4096, "bdv");
        v->sync();

        ASSERT_FALSE(fs::is_empty(*foc_ns_path));

        v->setFailOverCacheConfig(boost::none);

        EXPECT_FALSE(fs::exists(*foc_ns_path));
    }
}

// OVS-3430: a nasty race between flush and addEntries for the newData buffer
TEST_P(FailOverCacheTester, adding_and_flushing)
{
    auto foc_ctx(start_one_foc());
    auto wrns(make_random_namespace());

    SharedVolumePtr v = newVolume(*wrns);
    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    FailOverCacheBridgeInterface& foc = *v->getFailOver();

    std::atomic<bool> stop(false);
    size_t flushes = 0;

    auto future(std::async(std::launch::async,
                           [&]
                           {
                               while (not stop)
                               {
                                   foc.Flush().get();
                                   ++flushes;
                                   boost::this_thread::sleep_for(bc::milliseconds(5));
                               }
                           }));

    const size_t csize = v->getClusterSize();
    std::vector<byte> buf(csize);
    std::vector<ClusterLocation> locs(1);

    const size_t max = (1ULL << 27) / buf.size();
    const SCOOffset entries_per_sco = 1024;

    ASSERT_EQ(0, max % entries_per_sco);

    for (size_t i = 0; i < max; ++i)
    {
        *reinterpret_cast<size_t*>(buf.data()) = i;
        locs[0] = ClusterLocation(i / entries_per_sco + 1,
                                  i % entries_per_sco);

        while (true)
        {
            boost::future<void> f(foc.addEntries(locs,
                                                 i,
                                                 buf.data()));
            if (f.valid())
            {
                f.get();
                break;
            }
            else
            {
                boost::this_thread::sleep_for(bc::milliseconds(5));
            }
        }
    }

    EXPECT_LT(0, flushes);

    stop.store(true);

    future.wait();

    foc.Flush().get();

    size_t count = 0;

    for (size_t i = 1; i <= max / entries_per_sco; ++i)
    {
        foc.getSCOFromFailOver(SCO(i),
                               [&](ClusterLocation loc,
                                   uint64_t lba,
                                   const uint8_t* buf,
                                   size_t bufsize)
                               {
                                   ASSERT_EQ(count / entries_per_sco + 1, loc.sco().number());
                                   ASSERT_EQ(count % entries_per_sco, loc.offset());
                                   ASSERT_EQ(count, lba);
                                   ASSERT_EQ(count, *reinterpret_cast<const size_t*>(buf));
                                   ASSERT_EQ(csize, bufsize);
                                   ++count;
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

    std::unique_ptr<failovercache::ClientInterface>
        cache(failovercache::ClientInterface::create(foc_ctx->config(GetParam().foc_mode()),
                                                     wrns->ns(),
                                                     default_lba_size(),
                                                     default_cluster_multiplier(),
                                                     boost::chrono::milliseconds(60000),
                                                     boost::none));

    EXPECT_NO_THROW(cache->clear());

    const size_t csize = default_cluster_size();
    const std::vector<uint8_t> buf(csize);
    std::vector<FailOverCacheEntry> entries { FailOverCacheEntry(ClusterLocation(1),
                                                                 0,
                                                                 buf.data(),
                                                                 buf.size()) };
    cache->addEntries(std::move(entries)).get();

    EXPECT_NO_THROW(cache->clear());

    foc_ctx.reset();

    EXPECT_THROW(cache->clear(),
                 std::exception);
}

TEST_P(FailOverCacheTester, non_standard_cluster_size)
{
    auto foc_ctx(start_one_foc());
    auto wrns(make_random_namespace());
    const VolumeSize vsize(10ULL << 20);
    const ClusterMultiplier cmult(default_cluster_multiplier() * 2);
    const ClusterSize csize(default_lba_size() * cmult);

    SharedVolumePtr v = newVolume(*wrns,
                          vsize,
                          default_sco_multiplier(),
                          default_lba_size(),
                          cmult);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    SCOPED_BLOCK_BACKEND(*v);

    auto make_pattern([](size_t i) -> std::string
                      {
                          return std::string("cluster-" +
                                             boost::lexical_cast<std::string>(i));
                      });

    const size_t num_clusters = default_sco_multiplier() + 3;

    for (size_t i = 0; i < num_clusters; ++i)
    {
        writeToVolume(*v,
                      Lba(i * cmult),
                      csize,
                      make_pattern(i));
    }

    v->getFailOver()->Flush().get();

    std::unique_ptr<failovercache::ClientInterface>
        cache(failovercache::ClientInterface::create(foc_ctx->config(GetParam().foc_mode()),
                                                     wrns->ns(),
                                                     default_lba_size(),
                                                     cmult,
                                                     bc::milliseconds(60000),
                                                     boost::none));

    size_t count = 0;

    cache->getEntries([&](ClusterLocation /* loc */,
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

TEST_P(FailOverCacheTester, wrong_cluster_size)
{
    auto foc_ctx(start_one_foc());
    auto wrns(make_random_namespace());
    const VolumeSize vsize(10ULL << 20);
    const ClusterMultiplier cmult(default_cluster_multiplier() * 2);
    const ClusterSize csize(default_lba_size() * cmult);

    SharedVolumePtr v = newVolume(*wrns,
                          vsize,
                          default_sco_multiplier(),
                          default_lba_size(),
                          cmult);

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    SCOPED_BLOCK_BACKEND(*v);

    const std::string pattern("cluster");
    writeToVolume(*v,
                  Lba(0),
                  csize,
                  pattern);

    v->getFailOver()->Flush().get();

    EXPECT_THROW(failovercache::ClientInterface::create(foc_ctx->config(GetParam().foc_mode()),
                                                        wrns->ns(),
                                                        LBASize(default_lba_size()),
                                                        ClusterMultiplier(default_cluster_multiplier()),
                                                        bc::milliseconds(60000),
                                                        boost::none),
                 std::exception);
}

TEST_P(FailOverCacheTester, DISABLED_connect_timeout)
{
    auto wrns(make_random_namespace());

    const FailOverCacheConfig cfg("192.168.23.7",
                                  23000,
                                  GetParam().foc_mode());

    auto test([&](const boost::optional<bc::milliseconds>& connect_timeout)
              {
                  yt::SteadyTimer t;

                  EXPECT_THROW(failovercache::ClientInterface::create(cfg,
                                                                      wrns->ns(),
                                                                      default_lba_size(),
                                                                      default_cluster_multiplier(),
                                                                      bc::milliseconds(1000),
                                                                      connect_timeout),
                               std::exception);

                  std::cout << "connection attempt with connect timeout of " <<
                      connect_timeout << " took " << bc::duration_cast<bc::milliseconds>(t.elapsed()) <<
                      std::endl;
              });

    test(boost::none);
    test(bc::milliseconds(1000));
}

TEST_P(FailOverCacheTester, DISABLED_a_whole_lotta_clients)
{
    auto foc_ctx(start_one_foc());

    const size_t count = 1024;
    std::vector<std::unique_ptr<be::BackendTestSetup::WithRandomNamespace>> wrns;
    wrns.reserve(count);

    std::vector<std::unique_ptr<failovercache::ClientInterface>> clients;
    clients.reserve(count);

    try
    {
        for (size_t i = 0; i < 512; ++i)
        {
            wrns.emplace_back(make_random_namespace());
            clients.emplace_back(failovercache::ClientInterface::create(foc_ctx->config(GetParam().foc_mode()),
                                                                        wrns.back()->ns(),
                                                                        default_lba_size(),
                                                                        default_cluster_multiplier(),
                                                                        bc::milliseconds(1000),
                                                                        boost::none));
        }

        FAIL() << "eventually the DTL should've run out of FDs";
    }
    catch (const fungi::IOException& e)
    {
        EXPECT_EQ(EMFILE,
                  e.getErrorCode());
    }
}

// Might be better suited in SimpleVolumeTest or sth. similar?
TEST_P(FailOverCacheTester, dtl_in_sync)
{
    auto wrns = make_random_namespace();
    auto foc_ctx(start_one_foc());

    const FailOverCacheMode mode = GetParam().foc_mode();

    SharedVolumePtr v = newVolume(*wrns);
    const std::vector<uint8_t> buf(v->getClusterSize(), 42);

    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());
    EXPECT_EQ(DtlInSync::F,
              v->write(Lba(0), buf.data(), buf.size()));
    EXPECT_EQ(DtlInSync::F,
              v->sync());

    {
        SCOPED_BLOCK_BACKEND(*v);

        v->setFailOverCacheConfig(foc_ctx->config(mode));

        EXPECT_EQ(DtlInSync::F,
                  v->write(Lba(0), buf.data(), buf.size()));
        EXPECT_EQ(DtlInSync::F,
                  v->sync());
    }

    v->scheduleBackendSync();
    waitForThisBackendWrite(*v);

    ASSERT_EQ(VolumeFailOverState::OK_SYNC,
              v->getVolumeFailOverState());

    const DtlInSync dtl_in_sync = v->write(Lba(0), buf.data(), buf.size());

    if (mode == FailOverCacheMode::Synchronous)
    {
        EXPECT_EQ(DtlInSync::T,
                  dtl_in_sync);
    }
    else
    {
        EXPECT_EQ(DtlInSync::F,
                  dtl_in_sync);
    }

    EXPECT_EQ(DtlInSync::T,
              v->sync());
}

// needs to be run as root, and messes with the iptables, so use with _extreme_
// caution
/*
TEST_P(FailOverCacheTester, DISABLED_tarpit)
{
    ASSERT_EQ(0, ::geteuid()) << "test needs to run with root privileges!";


    auto foc_ctx(start_one_foc());


    SharedVolumePtr v = newVolume("volume",
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
        writeToVolume(*v,
                      0,
                      v->getClusterSize(),
                      pattern);
    }

    createSnapshot(v, "snap");
    waitForThisBackendWrite(*v);
    waitForThisBackendWrite(*v);

    EXPECT_EQ(VolumeInterface::VolumeState::DEGRADED, v->getVolumeFailOverState());

    SharedVolumePtr c = createClone("clone",
                            "cloneNamespace",
                            "volumeNamespace",
                            "snap");

    checkVolume(*c,
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

const VolumeDriverTestConfig sync_foc_in_memory_config =
    VolumeDriverTestConfig()
    .use_cluster_cache(true)
    .foc_mode(FailOverCacheMode::Synchronous)
    .foc_in_memory(true);
}

INSTANTIATE_TEST_CASE_P(FailOverCacheTesters,
                        FailOverCacheTester,
                        ::testing::Values(cluster_cache_config,
                                          sync_foc_config,
                                          sync_foc_in_memory_config));

}

// Local Variables: **
// mode: c++ **
// End: **
