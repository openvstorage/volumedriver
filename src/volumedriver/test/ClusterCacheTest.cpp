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

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <boost/filesystem/fstream.hpp>

#include <youtils/DimensionedValue.h>
#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/ScopeExit.h>
#include <youtils/wall_timer.h>

#include "../Api.h"
#include "../VolManager.h"

OUR_STRONG_ARITHMETIC_TYPEDEF(uint64_t, Devices, volumedrivertest);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint64_t, Hits, volumedrivertest);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint64_t, Misses, volumedrivertest);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint64_t, Entries, volumedrivertest);

namespace volumedrivertest
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace yt = youtils;

using namespace volumedriver;

//can be done more elegant? -> only used within CHECK_STATS macro
void check_stats_typecheck(Devices,
                           Hits,
                           Misses,
                           Entries)
{
}

#define CHECK_STATS(expected_devices,                                   \
                    expected_hits,                                      \
                    expected_misses,                                    \
                    expected_entries)                                   \
    {                                                                   \
        check_stats_typecheck(expected_devices,                         \
                              expected_hits,                            \
                              expected_misses,                          \
                              expected_entries);                        \
        uint64_t hits = 0;                                              \
        uint64_t misses = 0;                                            \
        uint64_t entries = 0;                                           \
        ClusterCache::ManagerType::Info device_info;                    \
                                                                        \
        VolManager::get()->getClusterCache().deviceInfo(device_info);   \
                                                                        \
        EXPECT_EQ(expected_devices, device_info.size());                \
        VolManager::get()->getClusterCache().get_stats(hits,            \
                                                       misses,          \
                                                       entries);        \
        EXPECT_EQ(expected_hits, Hits(hits));                           \
        EXPECT_EQ(expected_misses, misses);                             \
        EXPECT_EQ(expected_entries, entries);                           \
    }

class ClusterCacheTest
    : public VolManagerTestSetup
{
public:
    ClusterCacheTest()
        : VolManagerTestSetup("ClusterCacheTest")
    {}

    void
    triggerDeviceOnlining()
    {
        boost::property_tree::ptree pt;
        VolManager::get()->persistConfiguration(pt);
        UpdateReport u_rep;
        ConfigurationReport c_rep;
        VolManager::get()->updateConfiguration(pt,
                                               u_rep,
                                               c_rep);
    }

    void
    testLocationBasedNoCache(SharedVolumePtr v)
    {
        const uint32_t test_size = 100;
        const uint32_t mod = 30;
        ASSERT_GT(test_size,
                  mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));

        writeClusters(*v, test_size, "_", mod);
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(test_size),
                    Entries(0));

        VolManager::get()->getClusterCache().fail_fd_forread();

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(2 * test_size),
                    Entries(0));

        triggerDeviceOnlining();

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(2 * test_size),
                    Entries(0));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(3 * test_size),
                    Entries(0));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(4 * test_size),
                    Entries(0));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(5 * test_size),
                    Entries(0));
    }

    TODO("AR: look into unifying all these test*CacheOn* helpers");
    void
    testLocationBasedCacheOnReadBehavior(SharedVolumePtr v,
                                         size_t test_size = 100)
    {
        const uint32_t mod = 30;
        ASSERT_GT(test_size,
                  mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));

        writeClusters(*v, test_size, "_", mod);
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(test_size),
                    Entries(test_size));

        VolManager::get()->getClusterCache().fail_fd_forread();

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(0),
                    Hits(0),
                    Misses(2 * test_size),
                    Entries(0));

        triggerDeviceOnlining();

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(2 * test_size),
                    Entries(0));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(3 * test_size),
                    Entries(test_size));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(test_size),
                    Misses(3 * test_size),
                    Entries(test_size));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(2 * test_size),
                    Misses(3 * test_size),
                    Entries(test_size));
    }

    void
    testCacheOnReadBehavior(SharedVolumePtr v,
                            size_t test_size = 100,
                            size_t mod = 30)
    {
        ASSERT_TRUE(test_size > mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));

        writeClusters(*v, test_size, "_", mod);
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));

        checkClusters(*v, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
        CHECK_STATS(Devices(2),
                    Hits(test_size - mod),
                    Misses(mod),
                    Entries(mod));
#else
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));
#endif

        VolManager::get()->getClusterCache().fail_fd_forread();

        checkClusters(*v, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
        CHECK_STATS(Devices(0),
                    Hits(test_size - mod),
                    Misses(mod + test_size),
                    Entries(0));
#else
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));
#endif

        triggerDeviceOnlining();

#ifdef ENABLE_MD5_HASH
        CHECK_STATS(Devices(2),
                    Hits(test_size - mod),
                    Misses(mod + test_size),
                    Entries(0));
#else
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));
#endif

        checkClusters(*v, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
        CHECK_STATS(Devices(2),
                    Hits(2 * (test_size - mod)),
                    Misses(2 * mod + test_size),
                Entries(mod));
#else
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));
#endif
    }

    void
    testLocationBasedCacheOnWriteBehavior(SharedVolumePtr v,
                                          size_t test_size = 100)
    {
        const uint32_t mod = 30;
        ASSERT_GT(test_size,
                  mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));

        writeClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(test_size));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(test_size),
                    Misses(0),
                    Entries(test_size));

        VolManager::get()->getClusterCache().fail_fd_forread();

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(0),
                    Hits(test_size),
                    Misses(test_size),
                    Entries(0));

        triggerDeviceOnlining();

        CHECK_STATS(Devices(2),
                    Hits(test_size),
                    Misses(test_size),
                    Entries(0));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(test_size),
                    Misses(2 * test_size),
                    Entries(test_size));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(2 * test_size),
                    Misses(2 * test_size),
                    Entries(test_size));

        checkClusters(*v, test_size, "_", mod);

        CHECK_STATS(Devices(2),
                    Hits(3 * test_size),
                    Misses(2 * test_size),
                    Entries(test_size));
    }

    void
    testCacheOnWriteBehavior(SharedVolumePtr v,
                             size_t test_size = 100)
    {
        const uint32_t mod = 30;
        ASSERT_GT(test_size, mod);

        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));

        writeClusters(*v, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(mod));
#else
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));
#endif

        checkClusters(*v, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
        CHECK_STATS(Devices(2),
                    Hits(test_size),
                    Misses(0),
                    Entries(mod));
#else
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));
#endif

        VolManager::get()->getClusterCache().fail_fd_forread();

        checkClusters(*v, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
        CHECK_STATS(Devices(0),
                    Hits(test_size),
                    Misses(test_size),
                    Entries(0));
#else
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));
#endif

        triggerDeviceOnlining();

#ifdef ENABLE_MD5_HASH
        CHECK_STATS(Devices(2),
                    Hits(test_size),
                    Misses(test_size),
                    Entries(0));
#else
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));
#endif

        checkClusters(*v, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
        CHECK_STATS(Devices(2),
                    Hits(2* (test_size ) - mod),
                    Misses(mod + test_size),
                    Entries(mod));
#else
        CHECK_STATS(Devices(2),
                    Hits(0),
                    Misses(0),
                    Entries(0));
#endif
    }

    decltype(ClusterCache::ManagerType::devices)&
    get_cache_devices(ClusterCache& cache)
    {
        return cache.manager_.devices;
    }
};

TEST_P(ClusterCacheTest, sizes)
{
    std::cout << "sizeof KontentAddressedKacheEntry: " <<
        sizeof(ClusterCacheEntry) << std::endl;
}

TEST_P(ClusterCacheTest, cache_settings)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);
    EXPECT_FALSE(v1->isCacheOnWrite());
    EXPECT_TRUE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::CacheOnRead,
              VolManager::get()->get_cluster_cache_default_behaviour());

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::NoCache);

    EXPECT_FALSE(v1->isCacheOnWrite());
    EXPECT_FALSE(v1->isCacheOnRead());
    EXPECT_EQ(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::NoCache,
              VolManager::get()->get_cluster_cache_default_behaviour());

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::CacheOnWrite);

    EXPECT_TRUE(v1->isCacheOnWrite());
    EXPECT_FALSE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::CacheOnWrite,
              VolManager::get()->get_cluster_cache_default_behaviour());

    v1->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnWrite);

    EXPECT_TRUE(VolManager::get()->get_cluster_cache_default_behaviour() == ClusterCacheBehaviour::CacheOnWrite);
    EXPECT_TRUE(v1->isCacheOnWrite());
    EXPECT_FALSE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::CacheOnWrite,
              VolManager::get()->get_cluster_cache_default_behaviour());

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::NoCache);
    EXPECT_TRUE(v1->isCacheOnWrite());
    EXPECT_FALSE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::NoCache,
              VolManager::get()->get_cluster_cache_default_behaviour());

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::CacheOnRead);
    EXPECT_TRUE(v1->isCacheOnWrite());
    EXPECT_FALSE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::CacheOnRead,
              VolManager::get()->get_cluster_cache_default_behaviour());

    v1->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);
    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::CacheOnRead);
    EXPECT_FALSE(v1->isCacheOnWrite());
    EXPECT_TRUE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::CacheOnRead,
              VolManager::get()->get_cluster_cache_default_behaviour());

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::NoCache);
    EXPECT_FALSE(v1->isCacheOnWrite());
    EXPECT_TRUE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::NoCache,
              VolManager::get()->get_cluster_cache_default_behaviour());

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::CacheOnWrite);
    EXPECT_FALSE(v1->isCacheOnWrite());
    EXPECT_TRUE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::CacheOnWrite,
              VolManager::get()->get_cluster_cache_default_behaviour());

    v1->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnRead);

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::CacheOnRead);
    EXPECT_FALSE(v1->isCacheOnWrite());
    EXPECT_TRUE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::CacheOnRead,
              VolManager::get()->get_cluster_cache_default_behaviour());

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::NoCache);
    EXPECT_FALSE(v1->isCacheOnWrite());
    EXPECT_TRUE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::NoCache,
              VolManager::get()->get_cluster_cache_default_behaviour());

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::CacheOnWrite);
    EXPECT_FALSE(v1->isCacheOnWrite());
    EXPECT_TRUE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::CacheOnWrite,
              VolManager::get()->get_cluster_cache_default_behaviour());

    EXPECT_NO_THROW(v1->set_cluster_cache_mode(ClusterCacheMode::LocationBased));
    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::CacheOnRead);
    EXPECT_FALSE(v1->isCacheOnWrite());
    EXPECT_TRUE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::CacheOnRead,
              VolManager::get()->get_cluster_cache_default_behaviour());

    EXPECT_THROW(v1->set_cluster_cache_mode(ClusterCacheMode::ContentBased),
                 fungi::IOException);

    EXPECT_FALSE(v1->isCacheOnWrite());
    EXPECT_TRUE(v1->isCacheOnRead());
    EXPECT_NE(ClusterCacheBehaviour::NoCache,
              v1->effective_cluster_cache_behaviour());
    EXPECT_EQ(ClusterCacheBehaviour::CacheOnRead,
              VolManager::get()->get_cluster_cache_default_behaviour());
}

struct VolumeClusterCacheBehaviourSwitcher
{
    VolumeClusterCacheBehaviourSwitcher(SharedVolumePtr v)
        : v_(v)
        , stop_(false)
    {}

    void
    stop()
    {
        stop_ = true;
    }

    void
    operator()()
    {
        boost::optional<ClusterCacheBehaviour> behaviour_;
        while(not stop_)
        {
            //            LOG_FFATAL("volumedriver", "switching behaviour to " << behaviour_)
            v_->set_cluster_cache_behaviour(behaviour_);
            if(behaviour_)
            {

                if(behaviour_ == ClusterCacheBehaviour::NoCache)
                {
                    behaviour_ = ClusterCacheBehaviour::CacheOnRead;
                }
                else if(behaviour_ == ClusterCacheBehaviour::CacheOnRead)
                {
                    behaviour_ = ClusterCacheBehaviour::CacheOnWrite;
                }
                else if(behaviour_ == ClusterCacheBehaviour::CacheOnWrite)
                {
                    behaviour_ = boost::none;
                }
                else
                {
                    throw "UnexpectedClusterCacheBehaviourValue";
                }
            }
            else
            {
                behaviour_ = ClusterCacheBehaviour::NoCache;
            }
            usleep(10);
        }
    }

    SharedVolumePtr v_;
    bool stop_;
};

TEST_P(ClusterCacheTest, zimultanious)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);
    ASSERT_FALSE(v1->get_cluster_cache_behaviour());

    VolumeClusterCacheBehaviourSwitcher p(v1);
    boost::thread t(boost::ref(p));

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         p.stop();
                                         t.join();
                                     }));

    int32_t test_size = 100;
    int32_t mod = 30;
    ASSERT_TRUE(test_size > mod);
    youtils::wall_timer timer;
    double test_duration_in_seconds = 10;


    while(timer.elapsed() < test_duration_in_seconds)
    {
        writeClusters(*v1, test_size, "_", mod);
        checkClusters(*v1, test_size, "_", mod);
    }
}

TEST_P(ClusterCacheTest, offline1_defaultRead)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);
    ASSERT_FALSE(v1->get_cluster_cache_behaviour());
    ASSERT_TRUE(VolManager::get()->get_cluster_cache_default_behaviour() == ClusterCacheBehaviour::CacheOnRead);
    testCacheOnReadBehavior(v1,
                            100,
                            30);
}

TEST_P(ClusterCacheTest, offline1_cacheOnRead)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);
    boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnRead;
    v1->set_cluster_cache_behaviour(behaviour);
    testCacheOnReadBehavior(v1);
}

TEST_P(ClusterCacheTest, checkCacheBehaviour)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();
    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);
    {

        boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnWrite;
        v1->set_cluster_cache_behaviour(behaviour);
        ASSERT_TRUE(v1->get_cluster_cache_behaviour() == behaviour);
    }

    auto ns2_ptr = make_random_namespace();
    const Namespace& ns2 = ns2_ptr->ns();

    SharedVolumePtr v2 = newVolume("volume2",
                           ns2);
    {
        boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnRead;
        v2->set_cluster_cache_behaviour(behaviour);
        ASSERT_TRUE(v2->get_cluster_cache_behaviour() == behaviour);
    }

    int32_t test_size = 100;
    int32_t mod = 30;
    ASSERT_TRUE(test_size > mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));

    writeClusters(*v1, test_size, "___", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(mod));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    writeClusters(*v2, test_size, "***", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(mod));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    checkClusters(*v2, test_size, "***", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(test_size - mod),
                Misses(mod),
                Entries(2 * mod));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    writeClusters(*v2, test_size, "___", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(test_size - mod),
                Misses(mod),
                Entries(2 * mod));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    checkClusters(*v2, test_size, "___", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(2 * test_size - mod),
                Misses(mod),
                Entries(2 * mod));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    {
        boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnRead;
        v1->set_cluster_cache_behaviour(behaviour);
        ASSERT_TRUE(v1->get_cluster_cache_behaviour() == behaviour);
    }
    {
        boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnWrite;
        v2->set_cluster_cache_behaviour(behaviour);
        ASSERT_TRUE(v2->get_cluster_cache_behaviour() == behaviour);
    }

    writeClusters(*v2, test_size, "$$$", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(2 * test_size - mod),
                Misses(mod),
                Entries(3 * mod));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    writeClusters(*v1, test_size, "$$$", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(2 * test_size - mod),
                Misses(mod),
                Entries(3 * mod));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    checkClusters(*v2, test_size, "$$$", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(3 * test_size - mod),
                Misses(mod),
                Entries(3 * mod));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif
}

TEST_P(ClusterCacheTest, LocationBased_checkCacheBehaviour)
{
    boost::optional<ClusterCacheMode> mode = ClusterCacheMode::LocationBased;
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();
    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);
    {

        boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnWrite;
        v1->set_cluster_cache_behaviour(behaviour);
        v1->set_cluster_cache_mode(mode);
        ASSERT_TRUE(v1->get_cluster_cache_behaviour() == behaviour);
        ASSERT_TRUE(v1->get_cluster_cache_mode() == mode);
    }

    auto ns2_ptr = make_random_namespace();
    const Namespace& ns2 = ns2_ptr->ns();

    SharedVolumePtr v2 = newVolume("volume2",
                           ns2);
    {
        boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnRead;
        v2->set_cluster_cache_behaviour(behaviour);
        v2->set_cluster_cache_mode(mode);
        ASSERT_TRUE(v2->get_cluster_cache_behaviour() == behaviour);
        ASSERT_TRUE(v2->get_cluster_cache_mode() == mode);
    }

    int32_t test_size = 100;
    int32_t mod = 30;
    ASSERT_TRUE(test_size > mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));

    writeClusters(*v1, test_size, "___", mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(test_size));

    writeClusters(*v2, test_size, "***", mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(test_size));

    checkClusters(*v2, test_size, "***", mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(test_size),
                Entries(2 * test_size));

    writeClusters(*v2, test_size, "___", mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(test_size),
                Entries(test_size));

    checkClusters(*v2, test_size, "___", mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(2 * test_size),
                Entries(2 * test_size));

    {
        boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnRead;
        v1->set_cluster_cache_behaviour(behaviour);
        ASSERT_TRUE(v1->get_cluster_cache_behaviour() == behaviour);
    }
    {
        boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnWrite;
        v2->set_cluster_cache_behaviour(behaviour);
        ASSERT_TRUE(v2->get_cluster_cache_behaviour() == behaviour);
    }

    /* cache entries are muttable for location based hashing */
    writeClusters(*v2, test_size, "$$$", mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(2 * test_size),
                Entries(2 * test_size));

    /* invalidate entries for CacheOnRead */
    writeClusters(*v1, test_size, "$$$", mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(2 * test_size),
                Entries(test_size));

    checkClusters(*v2, test_size, "$$$", mod);

    CHECK_STATS(Devices(2),
                Hits(test_size),
                Misses(2 * test_size),
                Entries(test_size));
}

TEST_P(ClusterCacheTest, LocationBased_cacheOnWrite)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnWrite;
    boost::optional<ClusterCacheMode> mode = ClusterCacheMode::LocationBased;

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);

    v1->set_cluster_cache_behaviour(behaviour);
    v1->set_cluster_cache_mode(mode);

    ASSERT_TRUE(v1->get_cluster_cache_behaviour() == behaviour);
    ASSERT_TRUE(v1->get_cluster_cache_mode() == mode);

    testLocationBasedCacheOnWriteBehavior(v1);
}

TEST_P(ClusterCacheTest, LocationBased_cacheOnRead)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnRead;
    boost::optional<ClusterCacheMode> mode = ClusterCacheMode::LocationBased;

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);

    v1->set_cluster_cache_behaviour(behaviour);
    v1->set_cluster_cache_mode(mode);

    ASSERT_TRUE(v1->get_cluster_cache_behaviour() == behaviour);
    ASSERT_TRUE(v1->get_cluster_cache_mode() == mode);

    testLocationBasedCacheOnReadBehavior(v1);
}

TEST_P(ClusterCacheTest, LocationBased_noCache)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::NoCache;
    boost::optional<ClusterCacheMode> mode = ClusterCacheMode::LocationBased;

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);

    v1->set_cluster_cache_behaviour(behaviour);
    v1->set_cluster_cache_mode(mode);

    ASSERT_TRUE(v1->get_cluster_cache_behaviour() == behaviour);
    ASSERT_TRUE(v1->get_cluster_cache_mode() == mode);

    testLocationBasedNoCache(v1);
}

TEST_P(ClusterCacheTest, offline1_cacheOnWrite)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);
    boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::CacheOnWrite;
    v1->set_cluster_cache_behaviour(behaviour);
    ASSERT_TRUE(v1->get_cluster_cache_behaviour() == behaviour);

    testCacheOnWriteBehavior(v1);
}

TEST_P(ClusterCacheTest, offline1_defaultOnWrite)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);
    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::CacheOnWrite);
    ASSERT_FALSE(v1->get_cluster_cache_behaviour());
    ASSERT_TRUE(VolManager::get()->get_cluster_cache_default_behaviour() == ClusterCacheBehaviour::CacheOnWrite);

    testCacheOnWriteBehavior(v1);
}

TEST_P(ClusterCacheTest, offline1_nocache)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);
    boost::optional<ClusterCacheBehaviour> behaviour = ClusterCacheBehaviour::NoCache;
    v1->set_cluster_cache_behaviour(behaviour);

    int32_t test_size = 100;
    int32_t mod = 30;
    ASSERT_TRUE(test_size > mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));

    writeClusters(*v1, test_size, "_", mod);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));

    checkClusters(*v1, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(test_size),
                Entries(0));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    VolManager::get()->getClusterCache().fail_fd_forread();

    checkClusters(*v1, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(2 * test_size),
                Entries(0));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    triggerDeviceOnlining();

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(2 * test_size),
                Entries(0));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    checkClusters(*v1, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(3 * test_size),
                Entries(0));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif
}

TEST_P(ClusterCacheTest, offline2)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v1 = newVolume("volume1",
                           ns1);
    int32_t test_size = 100;
    int32_t mod = 30;

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));

    writeClusters(*v1, test_size, "_", mod);

    VolManager::get()->getClusterCache().fail_fd_forwrite();

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));

    checkClusters(*v1, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(0),
                Hits(0),
                Misses(test_size),
                Entries(0));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    checkClusters(*v1, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(0),
                Hits(0),
                Misses(2*test_size),
                Entries(0));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    triggerDeviceOnlining();

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(2*test_size),
                Entries(0));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    checkClusters(*v1, test_size, "_", mod);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(test_size-mod),
                Misses(2*test_size + mod),
                Entries(mod));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif
}

TEST_P(ClusterCacheTest, offline_online)
{
    fungi::ScopedLock __l(api::getManagementMutex());
    CHECK_STATS(Devices(2), Hits(0), Misses(0), Entries(0));

    api::setClusterCacheDeviceOffline(cc_mp1_path_.string());
    CHECK_STATS(Devices(1), Hits(0), Misses(0), Entries(0));

    api::setClusterCacheDeviceOffline(cc_mp2_path_.string());
    CHECK_STATS(Devices(0), Hits(0), Misses(0), Entries(0));

    //twice offlining the same shouldn't be a problem
    ASSERT_NO_THROW(api::setClusterCacheDeviceOffline(cc_mp1_path_.string()));
    CHECK_STATS(Devices(0), Hits(0), Misses(0), Entries(0));

    api::setClusterCacheDeviceOnline(cc_mp1_path_.string());
    CHECK_STATS(Devices(1), Hits(0), Misses(0), Entries(0));

    //twice onlining the same shouldn't be a problem
    ASSERT_NO_THROW(api::setClusterCacheDeviceOnline(cc_mp1_path_.string()));
    CHECK_STATS(Devices(1), Hits(0), Misses(0), Entries(0));

    api::setClusterCacheDeviceOnline(cc_mp2_path_.string());
    CHECK_STATS(Devices(2), Hits(0), Misses(0), Entries(0));
}

TEST_P(ClusterCacheTest, offline_online_error_cases)
{
    fungi::ScopedLock __l(api::getManagementMutex());
    CHECK_STATS(Devices(2), Hits(0), Misses(0), Entries(0));

    ASSERT_THROW(api::setClusterCacheDeviceOffline("non-existing"), MountPointNotConfigured);
    ASSERT_THROW(api::setClusterCacheDeviceOnline("non-existing"), MountPointNotConfigured);

    api::setClusterCacheDeviceOffline(cc_mp1_path_.string());
    CHECK_STATS(Devices(1), Hits(0), Misses(0), Entries(0));
    fs::remove(cc_mp1_path_);
    ASSERT_THROW(api::setClusterCacheDeviceOnline(cc_mp1_path_.string()), fungi::IOException);
}

// Cf. OVS-1369:
// A bug in the deserialization of a mountpoint lead to garbage being left on the stream
// created from the archive which is then interpreted as the serialization of the next
// mountpoint.
TEST_P(ClusterCacheTest, device_awol_after_restart)
{
    fs::path cocked_up;
    fs::path unsoiled;

    {
        auto& devs(get_cache_devices(VolManager::get()->getClusterCache()));
        ASSERT_EQ(2U, devs.size());

        {
            const auto dev_info(devs.front()->info());
            cocked_up = dev_info.path;
        }

        {
            const auto dev_info(devs.back()->info());
            unsoiled = dev_info.path;
        }
    }

    bpt::ptree pt;
    VolManager::get()->persistConfiguration(pt);

    VolManager::stop();

    fs::remove_all(cocked_up);

    {
        // Remove the mountpoints from the configuration - we want 'em to be
        // deserialized.
        MountPointConfigs cfgs;
        ip::PARAMETER_TYPE(clustercache_mount_points)(cfgs).persist(pt);
    }

    VolManager::start(pt);

    auto& devs(get_cache_devices(VolManager::get()->getClusterCache()));
    ASSERT_EQ(1U, devs.size());

    const auto dev_info(devs.front()->info());
    ASSERT_EQ(unsoiled, dev_info.path);
}

TEST_P(ClusterCacheTest, namespaces)
{
    auto& cc = VolManager::get()->getClusterCache();

    {
        const std::vector<ClusterCacheHandle> nspaces(cc.list_namespaces());
        ASSERT_EQ(1U,
                  nspaces.size());
        ASSERT_EQ(ClusterCacheHandle(0),
                  nspaces.front());
    }

    const OwnerTag ctag(1);
    const ClusterCacheHandle chandle(cc.registerVolume(ctag,
                                                       ClusterCacheMode::ContentBased));
    ASSERT_EQ(ClusterCacheHandle(0),
              chandle);

    {
        const std::vector<ClusterCacheHandle> nspaces(cc.list_namespaces());
        ASSERT_EQ(1U,
                  nspaces.size());
        ASSERT_EQ(ClusterCacheHandle(0),
                  nspaces.front());
    }

    const OwnerTag ltag(2);
    const ClusterCacheHandle lhandle(cc.registerVolume(ltag,
                                                       ClusterCacheMode::LocationBased));
    ASSERT_NE(chandle,
              lhandle);

    {
        const std::vector<ClusterCacheHandle> nspaces(cc.list_namespaces());
        const std::set<ClusterCacheHandle> handles(nspaces.begin(),
                                                   nspaces.end());
        ASSERT_EQ(2U,
                  handles.size());
        ASSERT_TRUE(handles.find(chandle) != handles.end());
        ASSERT_TRUE(handles.find(lhandle) != handles.end());
    }

    cc.deregisterVolume(ltag);

    {
        const std::vector<ClusterCacheHandle> nspaces(cc.list_namespaces());
        ASSERT_EQ(1U,
                  nspaces.size());
        ASSERT_EQ(chandle,
                  nspaces.front());
    }

    cc.deregisterVolume(ltag);
    {
        const std::vector<ClusterCacheHandle> nspaces(cc.list_namespaces());
        ASSERT_EQ(1U,
                  nspaces.size());
        ASSERT_EQ(chandle,
                  nspaces.front());
    }

    cc.deregisterVolume(ctag);
    {
        const std::vector<ClusterCacheHandle> nspaces(cc.list_namespaces());
        ASSERT_EQ(1U,
                  nspaces.size());
        ASSERT_EQ(chandle,
                  nspaces.front());
    }
}

TEST_P(ClusterCacheTest, reregister)
{
    auto& cc = VolManager::get()->getClusterCache();

    const OwnerTag otag(1);
    const ClusterCacheHandle chandle(ClusterCacheHandle(0));
    const ClusterCacheHandle lhandle(cc.registerVolume(otag,
                                                       ClusterCacheMode::LocationBased));

    {
        const std::vector<ClusterCacheHandle> nspaces(cc.list_namespaces());
        const std::set<ClusterCacheHandle> handles(nspaces.begin(),
                                                   nspaces.end());
        ASSERT_EQ(2U,
                  handles.size());
        ASSERT_TRUE(handles.find(chandle) != handles.end());
        ASSERT_TRUE(handles.find(lhandle) != handles.end());
    }

    ASSERT_EQ(lhandle,
              cc.registerVolume(otag,
                                ClusterCacheMode::LocationBased));

    {
        const std::vector<ClusterCacheHandle> nspaces(cc.list_namespaces());
        const std::set<ClusterCacheHandle> handles(nspaces.begin(),
                                                   nspaces.end());
        ASSERT_EQ(2U,
                  handles.size());
        ASSERT_TRUE(handles.find(chandle) != handles.end());
        ASSERT_TRUE(handles.find(lhandle) != handles.end());
    }

    ASSERT_EQ(chandle,
              cc.registerVolume(otag,
                                ClusterCacheMode::ContentBased));

    {
        const std::vector<ClusterCacheHandle> nspaces(cc.list_namespaces());
        ASSERT_EQ(1U,
                  nspaces.size());
        ASSERT_EQ(chandle,
                  nspaces.front());
    }
}

TEST_P(ClusterCacheTest, limits)
{
    auto& cc = VolManager::get()->getClusterCache();

    const OwnerTag otag(1);
    const ClusterCacheHandle lhandle(cc.registerVolume(otag,
                                                       ClusterCacheMode::LocationBased));
    const ClusterCacheHandle chandle(cc.registerVolume(otag,
                                                       ClusterCacheMode::ContentBased));

    EXPECT_THROW(cc.get_max_entries(lhandle),
                 std::exception);
    EXPECT_THROW(cc.set_max_entries(lhandle,
                                    boost::optional<uint64_t>(0)),
                 std::exception);
    EXPECT_THROW(cc.set_max_entries(lhandle,
                                    boost::optional<uint64_t>(1)),
                 std::exception);

    EXPECT_EQ(boost::none,
              cc.get_max_entries(chandle));
    EXPECT_THROW(cc.set_max_entries(chandle,
                                    boost::optional<uint64_t>(0)),
                 std::exception);
    cc.set_max_entries(chandle,
                       boost::optional<uint64_t>(1));

    const boost::optional<uint64_t> max(cc.get_max_entries(chandle));
    ASSERT_NE(boost::none,
              max);
    EXPECT_EQ(1U,
              *max);
}

TEST_P(ClusterCacheTest, impose_limit_on_unlimited_namespace)
{
    const auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnWrite);
    v->set_cluster_cache_mode(ClusterCacheMode::LocationBased);

    auto& cc = VolManager::get()->getClusterCache();
    const size_t old_count = 16;

    const std::string s("Orpheus sat gloomy in his garden shed, wondering what to do");
    writeClusters(*v,
                  old_count,
                  s);

    const ClusterCacheHandle handle(v->getClusterCacheHandle());
    {
        const ClusterCache::NamespaceInfo ninfo(cc.namespace_info(handle));
        EXPECT_EQ(old_count,
                  ninfo.entries);
        EXPECT_EQ(boost::none,
                  ninfo.max_entries);
    }

    const size_t new_count = old_count / 2;
    ASSERT_NE(0U,
              new_count);

    cc.set_max_entries(handle,
                       new_count);

    {
        const ClusterCache::NamespaceInfo ninfo(cc.namespace_info(handle));
        EXPECT_EQ(new_count,
                  ninfo.entries);
        EXPECT_EQ(new_count,
                  *ninfo.max_entries);
    }

    checkClusters(*v,
                  old_count - new_count,
                  new_count,
                  s);

    CHECK_STATS(Devices(2),
                Hits(new_count),
                Misses(0),
                Entries(new_count));

    checkClusters(*v,
                  0,
                  old_count - new_count,
                  s);

    CHECK_STATS(Devices(2),
                Hits(new_count),
                Misses(old_count - new_count),
                Entries(new_count));
}

TEST_P(ClusterCacheTest, shrink_limited_namespace)
{
    const auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnWrite);
    v->set_cluster_cache_mode(ClusterCacheMode::LocationBased);

    auto& cc = VolManager::get()->getClusterCache();

    const ClusterCacheHandle handle(v->getClusterCacheHandle());
    const size_t old_count = 10;
    cc.set_max_entries(handle,
                       old_count);

    const std::string s("with a lump of wood and a piece of wire and a little pot of glue");
    writeClusters(*v,
                  old_count,
                  s);

    {
        const ClusterCache::NamespaceInfo ninfo(cc.namespace_info(handle));
        EXPECT_EQ(old_count,
                  ninfo.entries);
        EXPECT_EQ(old_count,
                  *ninfo.max_entries);
    }

    const size_t new_count = old_count / 2;
    ASSERT_NE(0U,
              new_count);

    cc.set_max_entries(handle,
                       new_count);

    {
        const ClusterCache::NamespaceInfo ninfo(cc.namespace_info(handle));
        EXPECT_EQ(new_count,
                  ninfo.entries);
        EXPECT_EQ(new_count,
                  *ninfo.max_entries);
    }

    checkClusters(*v,
                  old_count - new_count,
                  new_count,
                  s);

    CHECK_STATS(Devices(2),
                Hits(new_count),
                Misses(0),
                Entries(new_count));

    checkClusters(*v,
                  0,
                  old_count - new_count,
                  s);

    CHECK_STATS(Devices(2),
                Hits(new_count),
                Misses(old_count - new_count),
                Entries(new_count));
}

TEST_P(ClusterCacheTest, expand_limited_namespace)
{
    const auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnWrite);
    v->set_cluster_cache_mode(ClusterCacheMode::LocationBased);

    auto& cc = VolManager::get()->getClusterCache();

    const ClusterCacheHandle handle(v->getClusterCacheHandle());
    const size_t old_count = 10;
    cc.set_max_entries(handle,
                       old_count);

    const size_t new_count = old_count * 2;

    const std::string
        s("He sawed at the wood with half a heart and glued it to the bottom");
    writeClusters(*v,
                  new_count,
                  s);

    {
        const ClusterCache::NamespaceInfo ninfo(cc.namespace_info(handle));
        EXPECT_EQ(old_count,
                  ninfo.entries);
        EXPECT_EQ(old_count,
                  *ninfo.max_entries);
    }

    checkClusters(*v,
                  new_count - old_count,
                  old_count,
                  s);

    CHECK_STATS(Devices(2),
                Hits(old_count),
                Misses(0),
                Entries(old_count));

    cc.set_max_entries(handle,
                       new_count);

    checkClusters(*v,
                  0,
                  new_count - old_count,
                  s);

    CHECK_STATS(Devices(2),
                Hits(old_count),
                Misses(new_count - old_count),
                Entries(new_count));

    const std::string t("He strung a wire in between, he was feeling something rotten");
    writeClusters(*v,
                  new_count,
                  t);

    {
        const ClusterCache::NamespaceInfo ninfo(cc.namespace_info(handle));
        EXPECT_EQ(new_count,
                  ninfo.entries);
        EXPECT_EQ(new_count,
                  *ninfo.max_entries);
    }

    checkClusters(*v,
                  new_count,
                  t);

    CHECK_STATS(Devices(2),
                Hits(old_count + new_count),
                Misses(new_count - old_count),
                Entries(new_count));
}

TEST_P(ClusterCacheTest, make_limited_namespace_unlimited)
{
    const auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnWrite);
    v->set_cluster_cache_mode(ClusterCacheMode::LocationBased);

    auto& cc = VolManager::get()->getClusterCache();

    const ClusterCacheHandle handle(v->getClusterCacheHandle());
    const size_t old_count = 10;
    cc.set_max_entries(handle,
                       old_count);

    const std::string
        s("Orpheus looked at his instrument and he gave the wire a pluck");
    writeClusters(*v,
                  old_count,
                  s);

    {
        const ClusterCache::NamespaceInfo ninfo(cc.namespace_info(handle));
        EXPECT_EQ(old_count,
                  ninfo.entries);
        EXPECT_EQ(old_count,
                  *ninfo.max_entries);
    }

    cc.set_max_entries(handle,
                       boost::none);

    checkClusters(*v,
                  old_count,
                  s);

    CHECK_STATS(Devices(2),
                Hits(old_count),
                Misses(0),
                Entries(old_count));

    const std::string t("He heard a sound so beautiful, he gasped and said 'Oh my God'");

    const uint64_t new_count = 2 * old_count;

    writeClusters(*v,
                  new_count,
                  t);

    {
        const ClusterCache::NamespaceInfo ninfo(cc.namespace_info(handle));
        EXPECT_EQ(new_count,
                  ninfo.entries);
        EXPECT_EQ(boost::none,
                  ninfo.max_entries);
    }

    checkClusters(*v,
                  new_count,
                  t);

    CHECK_STATS(Devices(2),
                Hits(old_count + new_count),
                Misses(0),
                Entries(new_count));
}

TEST_P(ClusterCacheTest, tiny_location_based)
{
    const auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnWrite);
    v->set_cluster_cache_mode(ClusterCacheMode::LocationBased);

    const size_t count = 1;
    auto& cc = VolManager::get()->getClusterCache();

    cc.set_max_entries(v->getClusterCacheHandle(),
                       count);

    const std::string s("He rushed inside to tell his wife, he went racing down the halls");

    writeClusters(*v,
                  count,
                  s);

    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(count));

    checkClusters(*v,
                  count,
                  s);

    CHECK_STATS(Devices(2),
                Hits(count),
                Misses(0),
                Entries(count));
}


TEST_P(ClusterCacheTest, error_during_deserialization)
{
    const auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);
    const size_t csize = v->getClusterSize();

    v->set_cluster_cache_behaviour(ClusterCacheBehaviour::CacheOnWrite);

    const std::string s("Eurydice was still asleep in bed, like a sack of cannonballs");

    const size_t count = 1;
    writeClusters(*v,
                  count,
                  s);
    checkClusters(*v,
                  count,
                  s);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(count),
                Misses(0),
                Entries(count));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif

    ClusterCache::ManagerType::Info info;
    VolManager::get()->getClusterCache().deviceInfo(info);

    bpt::ptree pt;
    VolManager::get()->persistConfiguration(pt,
                                            ReportDefault::T);

    const ip::PARAMETER_TYPE(serialize_read_cache) serialize(pt);
    ASSERT_TRUE(serialize.value());

    temporarilyStopVolManager();

    const std::vector<uint8_t> buf(csize);

    for (const auto& i : info)
    {
        ClusterCacheDiskStore ds(i.first,
                                 i.second.total_size,
                                 GetParam().cluster_multiplier() * VolumeConfig::default_lba_size());

        for (size_t i = 0; i < ds.total_size() / buf.size(); ++i)
        {
            ds.write(buf.data(),
                     i);
        }
    }

    restartVolManager(pt);

    localRestart(wrns->ns());

    v = getVolume(VolumeId(wrns->ns().str()));

    checkClusters(*v,
                  count,
                  s);

#ifdef ENABLE_MD5_HASH
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(count),
                Entries(count));
#else
    CHECK_STATS(Devices(2),
                Hits(0),
                Misses(0),
                Entries(0));
#endif
}

TEST_P(ClusterCacheTest, invalidation_when_default_behaviour_is_no_cache)
{
    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::CacheOnWrite);
    set_cluster_cache_default_mode(ClusterCacheMode::LocationBased);

    const auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    const std::string
        s("'Look what I've made' cried Orpheus and he plucked a gentle note");

    const size_t count = 13;

    writeClusters(*v,
                  count,
                  s);

    checkClusters(*v,
                  count,
                  s);

    CHECK_STATS(Devices(2),
                Hits(count),
                Misses(0),
                Entries(count));

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::NoCache);

    const std::string
        t("Eurydice's eyes popped from their sockets and her tongue burst through her throat");

    writeClusters(*v,
                  count,
                  t);

    set_cluster_cache_default_behaviour(ClusterCacheBehaviour::CacheOnRead);

    checkClusters(*v,
                  count,
                  t);

    CHECK_STATS(Devices(2),
                Hits(count),
                Misses(count),
                Entries(count));
}

namespace
{

const ClusterMultiplier
big_cluster_multiplier(VolManagerTestSetup::default_test_config().cluster_multiplier() * 4);

const auto big_clusters_config = VolManagerTestSetup::default_test_config()
    .cluster_multiplier(big_cluster_multiplier);

}

INSTANTIATE_TEST_CASE_P(ClusterCacheTests,
                        ClusterCacheTest,
                        ::testing::Values(volumedriver::VolManagerTestSetup::default_test_config(),
                                          big_clusters_config));

}

// Local Variables: **
// mode: c++ **
// End: **
