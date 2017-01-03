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

#include "VolumeDriverTestConfig.h"
#include "LBAGenerator.h"
#include "VolManagerTestSetup.h"

#include <cassert>
#include <map>
#include <iostream>
#include <sstream>
#include <string>

#include <snappy.h>

#include <youtils/SourceOfUncertainty.h>
#include <youtils/System.h>
#include <youtils/UUID.h>
#include <youtils/wall_timer.h>
#include <youtils/Md5.h>

#include <backend/BackendInterface.h>

#include <../CachedMetaDataPage.h>
#include <../CachedMetaDataStore.h>
#include <../MetaDataStoreInterface.h>
#include <../SnapshotManagement.h>
#include <../TokyoCabinetMetaDataBackend.h>
#include <../Volume.h>

namespace volumedrivertest
{

using namespace volumedriver;

namespace yt = youtils;

class MetaDataStoreTest
    : public VolManagerTestSetup
{
protected:
    MetaDataStoreTest()
	: VolManagerTestSetup("MetaDataStoreTest0Volume")
        , w(growWeed())
    {}

    std::unique_ptr<MetaDataStoreInterface>&
    getMDStore(SharedVolumePtr v)
    {
        return v->metaDataStore_;
    }

    DECLARE_LOGGER("MetaDataStoreTest");
    youtils::Weed w;

    // Not so good as a test... you either test the mdstore in isolation
    // or you test writes in the context of a volume... this test mixes that up.
    void
    perfTest(uint64_t volsize,
             size_t stride,
             uint32_t cache_pages = 1024,
             uint32_t cork_size = 2048)
    {
        const VolumeId name("testvol");
        auto ns_ptr = make_random_namespace();

        const backend::Namespace& ns1 = ns_ptr->ns();

        // const backend::Namespace ns1;

        const auto params =
            VanillaVolumeConfigParameters(name,
                                          ns1,
                                          VolumeSize(volsize),
                                          new_owner_tag())
            .sco_multiplier(SCOMultiplier(4096))
            .cluster_multiplier(ClusterMultiplier(8))
            .metadata_cache_capacity(cache_pages);

        SharedVolumePtr v = newVolume(params);

        //    interval.start();
        MetaDataStoreInterface& md = *(v->metaDataStore_);

        {
            UUID cork;
            uint64_t written = 0;
            youtils::wall_timer interval;

            for (size_t i = 0; i < v->getSize() / v->getClusterSize(); i += stride)
            {

                if(i % cork_size == 0)
                {
                    cork = UUID();
                    md.cork(cork);
                }

                ClusterLocation loc(i + 1);
                youtils::Weed w = growWeed();
                ClusterLocationAndHash loc_and_hash(loc,w);
                md.writeCluster(i, loc_and_hash);
                ClusterLocationAndHash check;
                md.readCluster(i, check);
                EXPECT_EQ(loc, check.clusterLocation);
                ++written;
                if(i % cork_size == (cork_size - 1))
                {
                    md.unCork();
                }
            }

            LOG_INFO("Wrote " << written << " clusters in " <<
                     interval.elapsed() <<
                     " seconds: " << (written / interval.elapsed()) <<
                     " Clusters/s");
        }

        {
            uint64_t read = 0;
            youtils::wall_timer interval;

            for (size_t i = 0; i < v->getSize() / v->getClusterSize(); i += stride)
            {
                ClusterLocationAndHash loc;
                md.readCluster(i, loc);
                ClusterLocation ref(i + 1);
                EXPECT_EQ(ref, loc.clusterLocation);
                ++read;
            }

            LOG_INFO("Read " << read << " clusters in " <<
                     interval.elapsed() <<
                     " seconds: " << (read / interval.elapsed()) <<
                     " Clusters/s");
        }
    }

    void
    replay_perf_test(uint64_t vsize,
                     uint32_t num_tlogs,
                     uint32_t entries_per_tlog,
                     uint64_t backend_delay_us,
                     float randomness)
    {
        if (metadata_backend_type() == MetaDataBackendType::MDS)
        {
            return;
        }

        // mimicks the core of CachedMetaDataStore::CloneTLogs

        const VolumeId vname("testvol");
        const VolumeId name("testvol");
        auto ns_ptr = make_random_namespace();
        const backend::Namespace& nspace = ns_ptr->ns();
        // const backend::Namespace nspace;

        SharedVolumePtr v = newVolume(vname, nspace);
        auto md = dynamic_cast<CachedMetaDataStore*>(v->getMetaDataStore());

        ASSERT_TRUE(md != nullptr) << "use a CachedMetaDataStore";

        boost::unique_lock<decltype(CachedMetaDataStore::corks_lock_)>
            corkswg(md->corks_lock_);

        ASSERT_TRUE(md->cork_uuid_ == boost::none);
        ASSERT_TRUE(md->corks_.size() == 0 or
                    (md->corks_.size() == 1 and
                     md->corks_.front().second->empty()));

        LBAGenGen lgg(entries_per_tlog,
                      vsize,
                      randomness);
        std::unique_ptr<TLogGen>
            rg(new yt::RepeatGenerator<TLogGenItem, LBAGenGen>(lgg,
                                                               num_tlogs));

        std::unique_ptr<TLogGen>
            dg(new yt::DelayedGenerator<TLogGenItem>(std::move(rg),
                                                     backend_delay_us));

        auto ctr = std::make_shared<CombinedTLogReader>(std::move(dg));

        yt::wall_timer wt;

        md->processTLogReaderInterface(ctr, SCOCloneID(0));

        boost::unique_lock<decltype(CachedMetaDataStore::cache_lock_)>
            cachewg(md->cache_lock_);

        for (CachePage& p : md->page_list_)
        {
            md->maybeWritePage_locked_context(p, false);
        }

        std::cout <<
            "TLog replay (volume size " << vsize <<
            ", randomness " << randomness <<
            ", " << num_tlogs << " TLogs of " << entries_per_tlog <<
            " entries each, backend delay per TLog " << backend_delay_us <<
            " us) took " << wt.elapsed() << " seconds" << std::endl;
    }

    void
    write_instead_of_replay_perf_test(uint64_t vsize,
                                      uint32_t num_tlogs,
                                      uint32_t entries_per_tlog,
                                      uint32_t cork_size,
                                      uint64_t backend_delay_us,
                                      float randomness)
    {
        const VolumeId vname("testvol");
        // const backend::Namespace nspace;
        auto ns_ptr = make_random_namespace();

        const backend::Namespace& nspace = ns_ptr->ns();

        SharedVolumePtr v = newVolume(vname, nspace);
        auto& md = *(v->getMetaDataStore());

        LBAGenGen lgg(entries_per_tlog,
                      vsize,
                      randomness);

        std::unique_ptr<TLogGen>
            rg(new yt::RepeatGenerator<TLogGenItem, LBAGenGen>(lgg,
                                                               num_tlogs));

        std::unique_ptr<TLogGen>
            dg(new yt::DelayedGenerator<TLogGenItem>(std::move(rg),
                                                     backend_delay_us));

        std::unique_ptr<TLogGen>
            tg(new yt::ThreadedGenerator<TLogGenItem>(std::move(dg),
                                                      10));

        CombinedTLogReader ctr(std::move(tg));

        const Entry* e;
        uint64_t count = 0;
        UUID cork;

        yt::wall_timer wt;

        while ((e = ctr.nextLocation()))
        {
            if (count % cork_size == 0)
            {
                cork = UUID();
                md.cork(cork);
            }

            md.writeCluster(e->clusterAddress(),
                            e->clusterLocationAndHash());

            if(count % cork_size == (cork_size - 1))
            {
                md.unCork();
            }

            ++count;
        }

        std::cout <<
            "TLog replay (volume size " << vsize <<
            ", randomness " << randomness <<
            ", " << num_tlogs << " TLogs of " << entries_per_tlog <<
            " entries each, backend delay per TLog " << backend_delay_us <<
            "us , cork size " << cork_size <<
            ") took " << wt.elapsed() << " seconds" << std::endl;
    }

    void
    replay_without_tlog_reader_perf_test(uint64_t vsize,
                                         uint32_t num_tlogs,
                                         uint32_t entries_per_tlog,
                                         uint32_t cork_size,
                                         float randomness)
    {
        const VolumeId vname("testvol");
        // const backend::Namespace nspace;
        auto ns_ptr = make_random_namespace();

        const backend::Namespace& nspace = ns_ptr->ns();

        SharedVolumePtr v = newVolume(vname, nspace);
        auto& md = *(v->getMetaDataStore());

        const uint64_t total_size =
            num_tlogs * entries_per_tlog * default_cluster_size();

        LBAGenerator lg(randomness,
                        total_size,
                        vsize);

        UUID cork;
        const Entry* e;
        uint64_t count = 0;

        yt::wall_timer wt;

        while ((e = lg.nextAny()))
        {
            if (count % cork_size == 0)
            {
                cork = UUID();
                md.cork(cork);
            }

            md.writeCluster(e->clusterAddress(),
                            e->clusterLocationAndHash());

            if(count % cork_size == (cork_size - 1))
            {
                md.unCork();
            }

            ++count;
        }

        std::cout <<
            "TLog replay (volume size " << vsize <<
            ", randomness " << randomness <<
            ", " << num_tlogs << " TLogs of " << entries_per_tlog <<
            " entries each = " << count << " entries, cork size " << cork_size <<
            ") took " << wt.elapsed() << " seconds" << std::endl;
    }

    static size_t
    corked_clusters(const MetaDataStoreStats& mds)
    {
        size_t clusters = 0;
        for (const auto& p : mds.corked_clusters)
        {
            clusters += p.second;
        }

        return clusters;
    }
};

TEST_P(MetaDataStoreTest, syncStore)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr vol_ = newVolume("volume1",
                             ns);

    ASSERT_NO_THROW(vol_->getMetaDataStore()->sync());
}

TEST_P(MetaDataStoreTest, readUnwrittenCluster)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr vol_ = newVolume("volume1",
                             ns);

    ClusterLocationAndHash a;
    ASSERT_NO_THROW(vol_->getMetaDataStore()->readCluster(0, a));
    ASSERT_TRUE(a.clusterLocation.isNull());
}

TEST_P(MetaDataStoreTest, writeOnePage)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr vol_ = newVolume("volume1",
                             ns);

    ClusterLocation a(10,10,SCOCloneID(100),SCOVersion(100));


    ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(0,
                                                           ClusterLocationAndHash(a,w)));
}

TEST_P(MetaDataStoreTest, writeAndReadOnePage)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr vol_ = newVolume("volume1",
                             ns);

    ClusterLocation a(10,10,SCOCloneID(100),SCOVersion(100));


    ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(0,
                                                           ClusterLocationAndHash(a,w)));
    ClusterLocationAndHash b;
    ASSERT_NO_THROW(vol_->getMetaDataStore()->readCluster(0,
                                                          b));
    ASSERT_TRUE(a == b.clusterLocation);
}

TEST_P(MetaDataStoreTest, single_cache_page)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    const auto params =
        VanillaVolumeConfigParameters(VolumeId("volume"),
                                      ns,
                                      VolumeSize(10 << 20),
                                      new_owner_tag())
        .metadata_cache_capacity(1);

    SharedVolumePtr v = newVolume(params);

    auto md = v->getMetaDataStore();

    const ClusterLocationAndHash clh1(ClusterLocation(1), w);
    md->cork(UUID());
    md->writeCluster(0, clh1);
    md->unCork();

    md->cork(UUID());
    const ClusterLocationAndHash clh2(ClusterLocation(2), w);
    md->writeCluster(CachePage::capacity(), clh2);
    md->unCork();

    ClusterLocationAndHash clh;
    md->readCluster(0, clh);
    EXPECT_EQ(clh1.clusterLocation, clh.clusterLocation);

    md->readCluster(CachePage::capacity(), clh);
    EXPECT_EQ(clh2.clusterLocation, clh.clusterLocation);
}

TEST_P(MetaDataStoreTest, writeAndReadSeveralPages)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr vol_ = newVolume("volume1",
                             ns);

    const uint32_t offset = 1 << 14;
    for(uint32_t i =0; i < (30 * offset);  i +=offset){
        ClusterLocation a(i+1,10,SCOCloneID(100),SCOVersion(100));
        ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(i,
                                                                ClusterLocationAndHash(a,w)));
    }
    ASSERT_NO_THROW(vol_->getMetaDataStore()->sync());

    for(uint32_t i =0; i < (30 * offset);  i +=offset){
        ClusterLocation a(i+1,10,SCOCloneID(100),SCOVersion(100));
        ClusterLocationAndHash b;

        ASSERT_NO_THROW(vol_->getMetaDataStore()->readCluster(i,
                                                              b));
        ASSERT_TRUE(a == b.clusterLocation);
    }
}

TEST_P(MetaDataStoreTest, writeAndReadOnePage1)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr vol_ = newVolume("volume1",
                             ns);

    const uint32_t offset = 1 << 14;
    const uint32_t testsize = 100;

    for(uint32_t i =0; i < (testsize * offset);  i +=offset){
        ClusterLocation a(i+1,10, SCOCloneID(100), SCOVersion(100));
        ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(i,
                                                               ClusterLocationAndHash(a,w)));
    }
    ASSERT_NO_THROW(vol_->getMetaDataStore()->sync());

    for(uint32_t i =0; i < (testsize * offset);  i +=offset){
        ClusterLocation a(i+1,10, SCOCloneID(100),SCOVersion(100));

        ClusterLocationAndHash b;

        ASSERT_NO_THROW(vol_->getMetaDataStore()->readCluster(i,
                                                              b));
        EXPECT_TRUE(a == b.clusterLocation);
    }
}

TEST_P(MetaDataStoreTest, writeAndReadOnePage2)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr vol_ = newVolume("volume1",
			     ns);

    ClusterLocation a(10,10,SCOCloneID(100),SCOVersion(100));


    ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(0,
                                                           ClusterLocationAndHash(a,w)));
    ClusterLocationAndHash b;
    ASSERT_NO_THROW(vol_->getMetaDataStore()->readCluster(0,
                                                          b));
    EXPECT_TRUE(a == b.clusterLocation);
}

TEST_P(MetaDataStoreTest, writeAndReadSeveralPages2)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr vol_ = newVolume("volume1",
			     ns);

    const uint32_t offset = 1 << 14;
    const uint32_t testsize = 200;

    for(uint32_t i =0; i < (testsize * offset);  i +=offset){
        ClusterLocation loc(i+1,10,SCOCloneID(100),SCOVersion(100));
        ClusterLocationAndHash loc_and_hash(loc,w);

        ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(i,
                                                               loc_and_hash));
    }
    ASSERT_NO_THROW(vol_->getMetaDataStore()->sync());

    for(uint32_t i =0; i < (testsize * offset);  i +=offset){
        ClusterLocation a(i+1,10,SCOCloneID(100),SCOVersion(100));

        ClusterLocationAndHash b;

        ASSERT_NO_THROW(vol_->getMetaDataStore()->readCluster(i,
                                                              b));
        EXPECT_TRUE(a == b.clusterLocation);
    }
}

TEST_P(MetaDataStoreTest, writeAndReadSeveralPages3)
{

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr vol_ = newVolume("volume1",
			     ns);

    const uint32_t offset = 1 << 8;
    const uint32_t testsize = 2000;

    for(uint32_t i =0; i < (testsize * offset);  i +=offset){
        ClusterLocation loc(i+1,10,SCOCloneID(100),SCOVersion(100));
        ClusterLocationAndHash loc_and_hash(loc, w);
        ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(i,
                                                               loc_and_hash));
    }

    for(uint32_t i =0; i < (testsize * offset);  i +=offset){
        ClusterLocation loc(i+1,10,SCOCloneID(100),SCOVersion(100));
        ClusterLocationAndHash loc_and_hash(loc, w);
        ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(i,
                                                               loc_and_hash));
    }

    for(uint32_t i =13; i < (testsize * offset);  i +=offset){
        ClusterLocation loc(i,10,SCOCloneID(100),SCOVersion(100));
        ClusterLocationAndHash loc_and_hash(loc, w);
        ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(i,
                                                               loc_and_hash));
    }

    for(uint32_t i =13; i < (testsize * offset);  i +=offset){
        ClusterLocation loc(i,10,SCOCloneID(100),SCOVersion(100));
        ClusterLocationAndHash loc_and_hash(loc, w);
        ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(i,
                                                               loc_and_hash));
    }

    for(uint32_t i =37; i < (testsize * offset);  i +=offset){
        ClusterLocation loc(i,10,SCOCloneID(100),SCOVersion(100));
        ClusterLocationAndHash loc_and_hash(loc, w);
        ASSERT_NO_THROW(vol_->getMetaDataStore()->writeCluster(i,
                                                               loc_and_hash));
    }

    for(uint32_t i =0; i < (testsize * offset);  i +=offset){
        ClusterLocation a(i+1,10, SCOCloneID(100),SCOVersion(100));

        ClusterLocationAndHash b;

        ASSERT_NO_THROW(vol_->getMetaDataStore()->readCluster(i,
                                                              b));
        EXPECT_TRUE(a == b.clusterLocation);
    }
}

class Counter
    : public MetaDataStoreFunctor
{
public:
    virtual void
    operator()(ClusterAddress a,
               const ClusterLocationAndHash& l)
    {
        std::map<ClusterAddress, ClusterLocation>::const_iterator it;
        it = map.find(a);
        if(it != map.end())
        {
            throw "something";
        }
        else
        {
            map[a] = l.clusterLocation;
        }
    }

    std::map<ClusterAddress, ClusterLocation> map;
};

TEST_P(MetaDataStoreTest, DataStoreMap)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr vol_ = newVolume("volume1",
			     ns);

    // This depends on clustersize, sectorsize and such!!
    writeToVolume(*vol_, 0, 8192, "pattern");
    Counter c;
    vol_->scheduleBackendSync();
    waitForThisBackendWrite(*vol_);

    ASSERT_NO_THROW(vol_->getMetaDataStore()->for_each(c,
                                                       1000000));

    ASSERT_EQ(2U, c.map.size());

    writeToVolume(*vol_,0, 4096, "pattern");

    writeToVolume(*vol_, 13,4096,"pattern");

    c.map.clear();
    vol_->scheduleBackendSync();
    waitForThisBackendWrite(*vol_);

    ASSERT_NO_THROW(vol_->getMetaDataStore()->for_each(c,
                                                       100000));
    ASSERT_EQ(3U, c.map.size());

    writeToVolume(*vol_, 25,4096,"pattern");

    c.map.clear();
    vol_->scheduleBackendSync();
    waitForThisBackendWrite(*vol_);

    ASSERT_NO_THROW(vol_->getMetaDataStore()->for_each(c,
                                                       100000));
    ASSERT_EQ(5U, c.map.size());
}

TEST_P(MetaDataStoreTest, strided_volume_access)
{
    const VolumeSize vsize(youtils::System::get_env_with_default("VOLSIZE",
                                                                 VolumeSize(1ULL << 30)));
    const uint32_t stride(youtils::System::get_env_with_default<uint32_t>("STRIDE",
                                                                          1));
    const uint32_t pages(youtils::System::get_env_with_default<uint32_t>("MD_PAGES",
                                                                         1024));
    const uint32_t cork_distance(youtils::System::get_env_with_default<uint32_t>("CORK_DISTANCE",
                                                                                 128));

    perfTest(vsize, stride, pages, cork_distance);
}

TEST_P(MetaDataStoreTest, replay_performance)
{
    const uint64_t backend_delay_us = yt::System::get_env_with_default("BACKEND_DELAY_US",
                                                                       80000ULL);
    const uint32_t num_tlogs = yt::System::get_env_with_default("NUM_TLOGS",
                                                                32U);
    const uint32_t entries_per_tlog = yt::System::get_env_with_default("TLOG_ENTRIES",
                                                                       1U << 20);

    const uint64_t vsize = yt::System::get_env_with_default("VOLSIZE",
                                                            1ULL << 30);

    const float randomness = yt::System::get_env_with_default("RANDOMNESS",
                                                              0.0);

    replay_perf_test(vsize,
                     num_tlogs,
                     entries_per_tlog,
                     backend_delay_us,
                     randomness);
}

TEST_P(MetaDataStoreTest, write_instead_of_replay_performance)
{
    const uint64_t backend_delay_us = yt::System::get_env_with_default("BACKEND_DELAY_US",
                                                                       80000ULL);
    const uint32_t num_tlogs = yt::System::get_env_with_default("NUM_TLOGS",
                                                                32U);
    const uint32_t entries_per_tlog = yt::System::get_env_with_default("TLOG_ENTRIES",
                                                                       1U << 20);
    const uint32_t cork_size = yt::System::get_env_with_default("CORK_SIZE",
                                                                entries_per_tlog);
    const uint64_t vsize = yt::System::get_env_with_default("VOLSIZE",
                                                            1ULL << 30);
    const float randomness = yt::System::get_env_with_default("RANDOMNESS",
                                                              0.0);

    write_instead_of_replay_perf_test(vsize,
                                      num_tlogs,
                                      entries_per_tlog,
                                      cork_size,
                                      backend_delay_us,
                                      randomness);
}

TEST_P(MetaDataStoreTest, replay_without_tlog_reader_performance)
{
    const uint32_t num_tlogs = yt::System::get_env_with_default("NUM_TLOGS",
                                                                32U);
    const uint32_t entries_per_tlog = yt::System::get_env_with_default("TLOG_ENTRIES",
                                                                       1U << 20);
    const uint32_t cork_size = yt::System::get_env_with_default("CORK_SIZE",
                                                                entries_per_tlog);
    const uint64_t vsize = yt::System::get_env_with_default("VOLSIZE",
                                                            1ULL << 30);
    const float randomness = yt::System::get_env_with_default("RANDOMNESS",
                                                              0.0);

    replay_without_tlog_reader_perf_test(vsize,
                                         num_tlogs,
                                         entries_per_tlog,
                                         cork_size,
                                         randomness);
}

TEST_P(MetaDataStoreTest, DISABLED_256GiBVolumeStrideAccess)
{
    // page size of the metadata stores is 2048 at the moment - determine by
    // looking at the metadatastore isof hardwiring it
    perfTest(1ULL << 38, 2048);
}

TEST_P(MetaDataStoreTest, stats1)
{
    const uint32_t page_size = CachePage::capacity();
    const uint32_t max_pages = 3;
    const ClusterMultiplier cmult = default_cluster_multiplier();
    const LBASize sectorsize = default_lba_size();

    const uint64_t locs = (max_pages + 1) * page_size;
    const uint64_t volsize = locs * cmult * sectorsize;

    const auto ns(make_random_namespace());

    SharedVolumePtr v = newVolume("volume",
                          ns->ns(),
                          VolumeSize(volsize),
                          default_sco_multiplier(),
                          sectorsize,
                          cmult,
                          max_pages);

    std::unique_ptr<MetaDataStoreInterface>& md = getMDStore(v);

    MetaDataStoreStats mds;

    md->getStats(mds);
    EXPECT_EQ(0U, mds.used_clusters);
    EXPECT_EQ(0U, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(0U, corked_clusters(mds));

    for (uint64_t i = 0; i < locs; ++i)
    {
        ClusterLocation loc(i + 1);
        ClusterLocationAndHash clh(loc, w);
        md->writeCluster(i, clh);
    }

    md->getStats(mds);
    EXPECT_EQ(0U, mds.used_clusters);
    EXPECT_EQ(0U, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(locs, corked_clusters(mds));

    yt::UUID cork;
    md->cork(cork);
    md->unCork();

    md->getStats(mds);
    EXPECT_EQ(locs, mds.used_clusters);
    EXPECT_EQ(max_pages, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(0U, corked_clusters(mds));

    for (uint64_t i = 0; i < locs; ++i)
    {
        ClusterLocation loc(i + 1);
        ClusterLocationAndHash clh(loc, w);
        md->writeCluster(i, clh);
    }

    cork = yt::UUID();
    md->cork(cork);
    md->unCork();

    md->getStats(mds);
    EXPECT_EQ(locs, mds.used_clusters);
    EXPECT_EQ(max_pages, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(0U, corked_clusters(mds));
}

TEST_P(MetaDataStoreTest, stats2)
{
    const uint32_t page_size = CachePage::capacity();
    const uint32_t max_pages = 3;
    const ClusterMultiplier cmult = default_cluster_multiplier();
    const LBASize sectorsize = default_lba_size();

    const uint64_t locs = (max_pages + 1) * page_size;
    const uint64_t volsize = locs * cmult * sectorsize;

    const auto ns(make_random_namespace());

    SharedVolumePtr v = newVolume("volume",
                          ns->ns(),
                          VolumeSize(volsize),
                          default_sco_multiplier(),
                          sectorsize,
                          cmult,
                          max_pages);

    std::unique_ptr<MetaDataStoreInterface>& md = getMDStore(v);

    MetaDataStoreStats mds;

    md->getStats(mds);
    EXPECT_EQ(0U, mds.cache_hits);
    EXPECT_EQ(0U, mds.cache_misses);
    EXPECT_EQ(0U, mds.used_clusters);
    EXPECT_EQ(0U, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(0U, corked_clusters(mds));

    for (size_t i = 0; i < max_pages * page_size; i += page_size)
    {
        ClusterLocation loc(i + 1);
        ClusterLocationAndHash clh(loc, w);
        md->writeCluster(i, clh);
    }

    yt::UUID cork;
    md->cork(cork);
    md->unCork();

    md->getStats(mds);
    EXPECT_EQ(max_pages, mds.cache_misses);
    EXPECT_EQ(0U, mds.cache_hits);
    EXPECT_EQ(max_pages, mds.used_clusters);
    EXPECT_EQ(max_pages, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(0U, corked_clusters(mds));

    for (size_t i = 0; i < max_pages * page_size; i += page_size)
    {
        ClusterLocation loc(max_pages + i + 1);
        ClusterLocationAndHash clh(loc, w);
        md->writeCluster(i, clh);
    }

    cork = yt::UUID();
    md->cork(cork);
    md->unCork();

    md->getStats(mds);
    EXPECT_EQ(max_pages, mds.cache_misses);
    EXPECT_EQ(max_pages, mds.cache_hits);
    EXPECT_EQ(max_pages, mds.used_clusters);
    EXPECT_EQ(max_pages, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(0U, corked_clusters(mds));

    MetaDataStoreStats mds_copy = mds;

    for (size_t i = 0; i < max_pages * page_size; ++i)
    {
        ClusterLocation loc(2 * max_pages + i + 1);
        ClusterLocationAndHash clh(loc, w);
        md->writeCluster(i, clh);
    }

    cork = yt::UUID();
    md->cork(cork);
    md->unCork();

    md->getStats(mds);
    EXPECT_EQ(mds_copy.cache_misses, mds.cache_misses);
    EXPECT_EQ(mds_copy.cache_hits + (max_pages * page_size), mds.cache_hits);
    EXPECT_EQ(max_pages * page_size, mds.used_clusters);
    EXPECT_EQ(max_pages, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(0U, corked_clusters(mds));

    mds_copy = mds;

    for (size_t i = max_pages * page_size;
         i < locs;
         ++i)
    {
        ClusterLocation loc(2 * max_pages + i + 1);
        ClusterLocationAndHash clh(loc, w);
        md->writeCluster(i, clh);
    }

    cork = yt::UUID();
    md->cork(cork);
    md->unCork();

    md->getStats(mds);
    EXPECT_EQ(mds_copy.cache_misses + 1, mds.cache_misses);
    EXPECT_EQ(mds_copy.cache_hits + page_size - 1, mds.cache_hits);
    EXPECT_EQ(locs, mds.used_clusters);
    EXPECT_EQ(max_pages, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(0U, corked_clusters(mds));

    mds_copy = mds;

    for (size_t i = 0; i < locs; ++i)
    {
        ClusterLocation loc(3 * locs / page_size + i + 1);
        ClusterLocationAndHash clh(loc, w);
        md->writeCluster(i, clh);
    }

    cork = yt::UUID();
    md->cork(cork);
    md->unCork();

    md->getStats(mds);
    EXPECT_EQ(mds_copy.cache_misses + locs / page_size, mds.cache_misses);
    EXPECT_EQ(mds_copy.cache_hits + (page_size - 1) * locs / page_size,
              mds.cache_hits);
    EXPECT_EQ(locs, mds.used_clusters);
    EXPECT_EQ(max_pages, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(0U, corked_clusters(mds));

    mds_copy = mds;

    for (size_t i = 0; i < locs; ++i)
    {
        ClusterLocationAndHash clh;
        md->readCluster(i, clh);
    }

    md->getStats(mds);
    EXPECT_EQ(mds_copy.cache_misses + locs / page_size, mds.cache_misses);
    EXPECT_EQ(mds_copy.cache_hits + (page_size - 1) * locs / page_size,
              mds.cache_hits);
    EXPECT_EQ(locs, mds.used_clusters);
    EXPECT_EQ(max_pages, mds.cached_pages);
    EXPECT_EQ(max_pages, mds.max_pages);
    EXPECT_EQ(0U, corked_clusters(mds));
}

struct MetaDataCounter
{
    MetaDataCounter()
        : count_(0)
    {}

    void
    operator()(void*)
    {
        ++count_;
    }

    uint64_t count_;
};

TEST_P(MetaDataStoreTest, LRU)
{
    const uint32_t npages(youtils::System::get_env_with_default<uint32_t>("MD_PAGES",
                                                                          32));
    const uint64_t page_entries = CachePage::capacity();
    const uint64_t vsize = (npages + 1) * page_entries * default_cluster_size();

    const std::string vname("vol");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    // const backend::Namespace ns1;

    auto v = newVolume(vname,
                       ns1,
                       VolumeSize(vsize),
                       default_sco_multiplier(),
                       default_lba_size(),
                       default_cluster_multiplier(),
                       npages);

    MetaDataStoreInterface* md = v->getMetaDataStore();

    // TODO: use this for other tests as well.
    BOOST_STRONG_TYPEDEF(uint64_t, Hits);
    BOOST_STRONG_TYPEDEF(uint64_t, Misses);
    BOOST_STRONG_TYPEDEF(uint64_t, Entries);

    auto check_stats([&](Entries exp_pages,
                         Hits exp_hits,
                         Misses exp_misses)
                     {
                         MetaDataStoreStats mds;
                         md->getStats(mds);
                         EXPECT_EQ(exp_pages, Entries(mds.cached_pages));
                         EXPECT_EQ(exp_hits, Hits(mds.cache_hits));
                         EXPECT_EQ(exp_misses, Misses(mds.cache_misses));
                     });

    check_stats(Entries(0), Hits(0), Misses(0));

    {
        // fill the cache with pages 0 ... N
        for (uint64_t i = 0; i < npages; ++i)
        {
            const ClusterLocation loc(i + 1);
            const ClusterLocationAndHash clh(loc, w);
            md->writeCluster(i * page_entries, clh);
        }

        yt::UUID cork;
        md->cork(cork);
        md->unCork();
    }

    check_stats(Entries(npages), Hits(0), Misses(npages));

    // touch all pages from N ... 0 - in reverse order
    for (int64_t i = npages - 1; i >= 0; --i)
    {
        ClusterLocationAndHash clh;
        md->readCluster(i * page_entries, clh);
#ifdef ENABLE_MD5_HASH
        EXPECT_EQ(w, clh.weed());
#endif
        const ClusterLocation loc(i + 1);
        EXPECT_EQ(loc, clh.clusterLocation);
    }

    check_stats(Entries(npages), Hits(npages), Misses(npages));

    // get in another page M
    {
        const ClusterLocation loc(npages + 1);
        const ClusterLocationAndHash clh(loc, w);
        md->writeCluster(npages * page_entries, clh);

        yt::UUID cork;
        md->cork(cork);
        md->unCork();
    }

    check_stats(Entries(npages), Hits(npages), Misses(npages + 1));

    // read page N - this should've caused another miss
    {
        ClusterLocationAndHash clh;
        md->readCluster((npages - 1) * page_entries, clh);
#ifdef ENABLE_MD5_HASH
        EXPECT_EQ(w, clh.weed());
#endif
        const ClusterLocation loc(npages);
        EXPECT_EQ(loc, clh.clusterLocation);

        check_stats(Entries(npages), Hits(npages), Misses(npages + 2));
    }
}

TEST_P(MetaDataStoreTest, DISABLED_page_compression)
{
    const uint32_t num_pages(youtils::System::get_env_with_default("NUM_PAGES",
                                                                   1UL << 20));

    const float randomness = yt::System::get_env_with_default("RANDOMNESS",
                                                              0.0);

    const uint64_t total_entries = CachePage::capacity() * num_pages;
    yt::SourceOfUncertainty rand;
    std::vector<uint32_t> compressed_page_sizes(num_pages, CachePage::capacity());
    ClusterLocation prev(SCONumber(1));

    for (size_t i = 0; i < num_pages; ++i)
    {
        std::vector<ClusterLocationAndHash> data;
        data.reserve(CachePage::capacity());

        for (size_t j = 0; j < CachePage::capacity(); ++j)
        {
            ClusterLocation loc;

            const float x = rand(total_entries);
            if (x < randomness * total_entries)
            {
                loc = ClusterLocation(SCONumber(prev.number() + 1));
            }
            else
            {
                loc = ClusterLocation(SCONumber(rand(std::numeric_limits<uint32_t>::max())),
                                      SCOOffset(rand((uint32_t)VolumeConfig::default_sco_multiplier())),
                                      SCOCloneID(rand(std::numeric_limits<uint8_t>::max())),
                                      SCOVersion(rand(std::numeric_limits<uint8_t>::max())));
            }

            prev = loc;
            const uint64_t y = rand(std::numeric_limits<uint64_t>::max());
            yt::Weed weed(reinterpret_cast<const byte*>(&y),
                          sizeof(y));

            data.push_back(ClusterLocationAndHash(loc, weed));
        }


        std::string compressed;
        size_t res = snappy::Compress(reinterpret_cast<const char*>(data.data()),
                                      data.size() * sizeof(ClusterLocationAndHash),
                                      &compressed);
        ASSERT_LT(0U, res);

        compressed_page_sizes[i] = compressed.size();
    }

    uint64_t sum = 0;
    uint64_t max = std::numeric_limits<uint64_t>::min();
    uint64_t min = std::numeric_limits<uint64_t>::max();

    for (size_t i = 0; i < compressed_page_sizes.size(); ++i)
    {
        sum += compressed_page_sizes[i];
        if (max < compressed_page_sizes[i])
        {
            max = compressed_page_sizes[i];
        }

        if (compressed_page_sizes[i] < min)
        {
            min = compressed_page_sizes[i];
        }
    }

    std::cout << "num pages: " << num_pages <<
        ", page capacity: " << CachePage::capacity() <<
        ", page size: " << CachePage::capacity() * sizeof(ClusterLocationAndHash) <<
        ", randomness: " << randomness <<
        ", min size: " << min <<
        ", max size: " << max <<
        ", avg size: " << (sum * 1.0 / num_pages) <<
        std::endl;
}

TEST_P(MetaDataStoreTest, get_page)
{
    const uint32_t page_size = CachePage::capacity();
    const uint32_t max_pages = 3;
    const uint64_t locs = max_pages * page_size;
    const size_t csize = default_lba_size() * default_cluster_multiplier();
    const uint64_t volsize = locs * csize;

    const auto wrns(make_random_namespace());

    const auto params =
        VanillaVolumeConfigParameters(VolumeId(wrns->ns().str()),
                                      wrns->ns(),
                                      VolumeSize(volsize),
                                      new_owner_tag())
        .metadata_cache_capacity(max_pages)
        ;

    SharedVolumePtr v(newVolume(params));
    std::unique_ptr<MetaDataStoreInterface>& md = getMDStore(v);

    auto check([&]
               {
                   std::vector<ClusterLocation> vec;
                   for (ClusterAddress ca = 0; ca < locs; ++ca)
                   {
                       if ((ca % page_size) == 0)
                       {
                           vec = md->get_page(ca);
                       }

                       ASSERT_EQ(page_size,
                                 vec.size());

                       ClusterLocationAndHash clh;
                       md->readCluster(ca, clh);

                       PageAddress pa = CachePage::pageAddress(ca);
                       off_t off = CachePage::offset(ca);
                       EXPECT_EQ(clh.clusterLocation,
                                 vec.at(CachePage::offset(ca))) <<
                           "CA " << ca << ", PA " << pa << ", off " << off;
                   }
               });

    SCONumber sco_num = 1;

    auto write([&](size_t n)
               {
                   for (size_t i = 0; i < max_pages; ++i)
                   {
                       for (size_t j = 0; j < n; ++j)
                       {
                           ClusterLocationAndHash clh(ClusterLocation(sco_num++),
                                                      growWeed());
                           const ClusterAddress ca = i * page_size + j;
                           md->writeCluster(ca,
                                            clh);
                       }
                   }
               });
    check();

    write(4);
    check();

    md->cork(yt::UUID());

    write(2);
    check();
}

INSTANTIATE_TEST(MetaDataStoreTest);

}

// Local Variables: **
// mode: c++ **
// End: **
