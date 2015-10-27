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

#include <stdlib.h>

namespace volumedriver
{

namespace
{

struct CacheEntryProcessor
{
    CacheEntryProcessor()
        : num_clusters_(0)
    {}

    void
    processCluster(ClusterLocation /* loc */ ,
                   uint64_t /* lba */,
                   const byte* /* buf */,
                   size_t /* size */)
    {
        num_clusters_++;
    }

    uint64_t num_clusters_;
};

}

class FailOverCacheTester
    : public VolManagerTestSetup
{
public:
    FailOverCacheTester()
        :VolManagerTestSetup("FailOverCacheTester")
    {}

    void
    check_num_entries(Volume& v,
                      uint64_t expected)
    {
        auto bridge = getFailOverWriter(&v);

        // Otherwise we might clash with the FailOverCacheBridge's ping mechanism!
        fungi::ScopedLock g(bridge->getMutex());

        CacheEntryProcessor proc;
        bridge->getCache()->getEntries(SCOPROCESSORFUN(CacheEntryProcessor, processCluster, &proc));
        EXPECT_EQ(expected, proc.num_clusters_);
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
    v->setFailOverCacheConfig(foc_ctx->config());
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
    v->setFailOverCacheConfig(foc_ctx->config());

    {
        fungi::ScopedLock l(api::getManagementMutex());
        api::setFOCTimeout(vid,
                           100);
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
    v->setFailOverCacheConfig(foc_ctx->config());

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
                                                               FailOverCacheTestSetup::mode())),
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

        v->setFailOverCacheConfig(foc_ctx->config());

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

        v->setFailOverCacheConfig(foc_ctx->config());

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

        v->setFailOverCacheConfig(foc_ctx->config());


        ASSERT_TRUE(v->getVolumeFailOverState() == VolumeFailOverState::OK_SYNC);

        const std::string tlogname = v->getSnapshotManagement().getCurrentTLogName();

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

    ASSERT_TRUE(v->getVolumeFailOverState() == VolumeFailOverState::DEGRADED);

    {
        auto foc_ctx(start_one_foc());

        v->setFailOverCacheConfig(foc_ctx->config());

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
                                                               FailOverCacheTestSetup::mode())),
                  fungi::IOException);

    auto foc_ctx(start_one_foc());
    for(int i = 0; i < 4; ++i)
    {
        if(i % 2)
        {
            auto foc_ctx(start_one_foc());
            v->setFailOverCacheConfig(foc_ctx->config());
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

    v->setFailOverCacheConfig(foc_ctx->config());

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

    v->setFailOverCacheConfig(foc_ctx->config());

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

    v->setFailOverCacheConfig(foc_ctx->config());

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
    v->setFailOverCacheConfig(foc_ctx->config());

    const std::string tlogname1 = v->getSnapshotManagement().getCurrentTLogName();
    const unsigned entries = 32;

    for(unsigned i = 0; i < entries; i++)
    {
        writeToVolume(v, i * 4096, 4096, "bdv");
    }
    v->setFailOverCacheConfig(foc_ctx->config());

    const std::string tlogname2 = v->getSnapshotManagement().getCurrentTLogName();
    EXPECT_FALSE(tlogname1 == tlogname2);

    for(unsigned i = 0; i < entries; i++)
    {
        writeToVolume(v, i * 4096, 4096, "bdv");
    }

    flushFailOverCache(v);

    check_num_entries(*v, entries);

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


    const std::string tlogname1 = v->getSnapshotManagement().getCurrentTLogName();
    const unsigned entries = 32;

    for(unsigned i = 0; i < entries; i++)
    {
        writeToVolume(v, i * 4096, 4096, "bdv");
    }
    EXPECT_TRUE(v->getVolumeFailOverState() == VolumeFailOverState::OK_STANDALONE);

    EXPECT_NO_THROW(v->setFailOverCacheConfig(foc_ctx1->config()));

    EXPECT_NO_THROW(v->setFailOverCacheConfig(foc_ctx2->config()));

    EXPECT_NO_THROW(v->setFailOverCacheConfig(foc_ctx1->config()));

    EXPECT_TRUE(v->getVolumeFailOverState() == VolumeFailOverState::KETCHUP or
                v->getVolumeFailOverState() == VolumeFailOverState::OK_SYNC);

    for(unsigned i = 0; i < 10; ++i)
    {
        if(v->getVolumeFailOverState() == VolumeFailOverState::OK_SYNC)
            break;
        sleep(1);
    }

    EXPECT_TRUE(v->getVolumeFailOverState() == VolumeFailOverState::OK_SYNC);

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
                                                               FailOverCacheTestSetup::mode())),
                 fungi::IOException);

    ASSERT_TRUE(v->getVolumeFailOverState() == VolumeFailOverState::DEGRADED);
    auto foc_ctx(start_one_foc());
    ASSERT_TRUE(port == foc_ctx->port());

    sleep(2* failovercache_check_interval_in_seconds_);
    ASSERT_TRUE(v->getVolumeFailOverState() == VolumeFailOverState::OK_SYNC);

}

TEST_P(FailOverCacheTester, TLogsAreRemoved)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("vol1",
                          ns);

    v->setFailOverCacheConfig(foc_ctx->config());

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
    v->setFailOverCacheConfig(foc_ctx->config());

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

// needs to be run as root, and messes with the iptables, so use with _extreme_
// caution
/*
TEST_P(FailOverCacheTester, DISABLED_tarpit)
{
    ASSERT_EQ(0, ::geteuid()) << "test needs to run with root privileges!";


    auto foc_ctx(start_one_foc());


    Volume* v = newVolume("volume",
			  "volumeNamespace");

    v->setFailOverCache(foc_ctx->config());

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

INSTANTIATE_TEST(FailOverCacheTester);

}

// Local Variables: **
// mode: c++ **
// End: **
