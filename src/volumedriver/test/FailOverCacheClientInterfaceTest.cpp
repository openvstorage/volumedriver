// Copyright (C) 2017 iNuron NV
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
#include "VolumeDriverTestConfig.h"

#include "../failovercache/ClientInterface.h"
#include "../FailOverCacheEntry.h"
#include "../OwnerTag.h"
#include "../VolManager.h"

#include <future>

#include <youtils/AsioServiceManager.h>
#include <youtils/System.h>
#include <youtils/wall_timer.h>

namespace volumedrivertest
{

using namespace std::literals::string_literals;
using namespace volumedriver;
using namespace volumedriver::failovercache;

namespace ba = boost::asio;
namespace bc = boost::chrono;
namespace yt = youtils;

namespace
{

using EntryAndBuf = std::pair<FailOverCacheEntry, std::vector<uint8_t>>;

class Generator
{
public:
    Generator(size_t csize,
              SCOMultiplier sco_mult)
        : cluster_size_(csize)
        , sco_multiplier_(sco_mult)
    {}

    ~Generator() = default;

    Generator(const Generator&) = default;

    Generator&
    operator=(const Generator&) = default;

    EntryAndBuf
    operator()(Lba lba = Lba(0),
               const std::string& pattern = "")
    {
        std::vector<uint8_t> buf(cluster_size_);
        FailOverCacheEntry entry(operator()(buf.data(),
                                            buf.size(),
                                            lba,
                                            pattern));

        return std::make_pair(std::move(entry),
                              std::move(buf));
    }

    FailOverCacheEntry
    operator()(uint8_t* buf,
               const size_t bufsize,
               Lba lba = Lba(0),
               const std::string& pattern = "")
    {
        EXPECT_EQ(cluster_size_,
                  bufsize);

        FailOverCacheEntry e(loc_,
                             lba,
                             buf,
                             bufsize);

        if (loc_.offset() >= sco_multiplier_ - 1)
        {
            loc_.incrementNumber();
            loc_.offset(0);
        }
        else
        {
            loc_.incrementOffset();
        }

        if (not pattern.empty())
        {
            for (size_t i = 0; i < bufsize; ++i)
            {
                buf[i] = pattern[i % pattern.size()];
            }
        }

        return e;
    }

private:
    ClusterLocation loc_ = ClusterLocation(1);
    size_t cluster_size_;
    SCOMultiplier sco_multiplier_;
};

}

class FailOverCacheClientInterfaceTest
    : public VolManagerTestSetup
{
public:
    FailOverCacheClientInterfaceTest()
        : VolManagerTestSetup("FailOverCacheClientInterfaceTest")
        , owner_tag_(1)
    {}

    void
    SetUp() override final
    {
        VolManagerTestSetup::SetUp();
        wrns_ = make_random_namespace();
        foc_ctx_ = start_one_foc();

        std::shared_ptr<yt::AsioServiceManager>
            mgr(VolManager::get()->asio_service_manager());
        client_ = ClientInterface::create(mgr->get_service(0),
                                          mgr->implicit_strand(),
                                          foc_ctx_->config(GetParam().foc_mode()),
                                          wrns_->ns(),
                                          owner_tag_,
                                          default_lba_size(),
                                          default_cluster_multiplier(),
                                          boost::chrono::milliseconds(3000),
                                          boost::chrono::milliseconds(3000));
    }

    void
    TearDown() override final
    {
        client_ = nullptr;
        foc_ctx_ = nullptr;
        wrns_ = nullptr;
        VolManagerTestSetup::TearDown();
    }

    Generator
    make_generator()
    {
        return Generator(default_cluster_size(),
                         default_sco_multiplier());
    }

protected:
    const OwnerTag owner_tag_;
    std::unique_ptr<be::BackendTestSetup::WithRandomNamespace> wrns_;
    foctest_context_ptr foc_ctx_;
    std::unique_ptr<failovercache::ClientInterface> client_;
};

TEST_P(FailOverCacheClientInterfaceTest, empty_flush)
{
    client_->flush().get();
}

TEST_P(FailOverCacheClientInterfaceTest, empty_clear)
{
    client_->clear();
}

TEST_P(FailOverCacheClientInterfaceTest, empty_sco_range)
{
    SCO oldest;
    SCO youngest;

    client_->getSCORange(oldest, youngest);
    EXPECT_EQ(SCO(0),
              oldest);
    EXPECT_EQ(SCO(0),
              youngest);
}

TEST_P(FailOverCacheClientInterfaceTest, empty_get_entries)
{
    client_->getEntries([](ClusterLocation, Lba, const uint8_t*, size_t)
                        {
                            FAIL() << "there shouldn't be any entry!";
                        });
}

TEST_P(FailOverCacheClientInterfaceTest, empty_get_sco)
{
    EXPECT_EQ(0,
              client_->getSCOFromFailOver(SCO(1),
                                          [](ClusterLocation, Lba, const uint8_t*, size_t)
                                          {
                                              FAIL() << "there shouldn't be any entry!";
                                          }));
}

TEST_P(FailOverCacheClientInterfaceTest, empty_remove_up_to)
{
    client_->removeUpTo(SCO(1));
}

TEST_P(FailOverCacheClientInterfaceTest, add_and_get)
{
    const size_t nclusters = 8;
    ASSERT_LE(SCOMultiplier(nclusters),
              default_sco_multiplier()) << "addEntries must not cross SCO boundaries";

    const std::string pattern("something odd");
    EXPECT_NE(0,
              pattern.size() % 2);

    std::vector<uint8_t> vec(nclusters * default_cluster_multiplier() *
                             default_lba_size());
    for (size_t i = 0; i < vec.size(); ++i)
    {
        vec[i] = pattern[i % pattern.size()];
    }

    std::vector<ClusterLocation> locs(nclusters);
    for (SCOOffset i = 0; i < locs.size(); ++i)
    {
        locs[i] = ClusterLocation(1, i);
    }

    client_->addEntries(locs,
                        0,
                        vec.data()).get();
    client_->flush().get();

    size_t i = 0;
    client_->getEntries([&](ClusterLocation loc,
                            Lba lba,
                            const uint8_t* buf,
                            size_t bufsize)
                        {
                            EXPECT_EQ(locs[i],
                                      loc);
                            EXPECT_EQ(Lba(i * default_cluster_multiplier()),
                                      lba);
                            EXPECT_EQ(default_cluster_size(),
                                      bufsize);
                            EXPECT_EQ(0,
                                      memcmp(vec.data() + i * default_cluster_size(),
                                             buf,
                                             bufsize));
                            ++i;
                        });

    EXPECT_EQ(nclusters,
              i);
}

TEST_P(FailOverCacheClientInterfaceTest, the_full_monty)
{
    const size_t max =
        yt::System::get_env_with_default("DTL_CLIENT_INTERFACE_TEST_MAX_ENTRIES",
                                         1024UL);
    std::vector<EntryAndBuf> vec;
    vec.reserve(max);

    std::vector<boost::future<void>> futures;
    futures.reserve(max);

    auto gen(make_generator());

    for (size_t i = 0; i < max; ++i)
    {
        vec.push_back(gen(Lba(i * default_cluster_multiplier()),
                          "entry-"s + boost::lexical_cast<std::string>(i)));
        futures.push_back(client_->addEntries({ vec.back().first }));
    }

    client_->flush().get();
    for (auto& f : futures)
    {
        EXPECT_TRUE(f.is_ready());
        f.get();
    }

    SCO oldest;
    SCO youngest;

    client_->getSCORange(oldest,
                         youngest);
    EXPECT_EQ(vec.front().first.cli_.sco(),
              oldest);
    EXPECT_EQ(vec.back().first.cli_.sco(),
              youngest);

    size_t i = 0;

    auto fun([&](ClusterLocation loc,
                 Lba lba,
                 const uint8_t* buf,
                 size_t size)
             {
                 EXPECT_LT(i,
                           max);

                 EXPECT_EQ(default_cluster_size(),
                           size);
                 EXPECT_EQ(Lba(i * default_cluster_multiplier()),
                           lba);
                 EXPECT_EQ(vec[i].first.cli_,
                           loc);
                 EXPECT_EQ(0,
                           memcmp(vec[i].second.data(),
                                  buf,
                                  size)) << "i " << i << ", lba " << lba << ", loc " << loc;
                 ++i;
             });

    client_->getEntries(fun);

    EXPECT_EQ(i,
              max);

    i = 0;
    client_->getSCOFromFailOver(oldest,
                                fun);

    const size_t num_scos = youngest.number() - oldest.number();
    i = default_sco_multiplier() * num_scos;

    client_->getSCOFromFailOver(youngest,
                                fun);

    i = 0;
    client_->removeUpTo(oldest);
    client_->getSCOFromFailOver(oldest,
                                fun);
    EXPECT_EQ(0,
              i);

    {
        SCO o;
        SCO y;
        client_->getSCORange(o,
                             y);
        if (num_scos)
        {
            EXPECT_EQ(SCO(oldest.number() + 1),
                      o);
            EXPECT_EQ(youngest,
                      y);

            i = default_sco_multiplier();
            client_->getEntries(fun);
            EXPECT_EQ(max,
                      i);
        }
        else
        {
            EXPECT_EQ(SCO(0),
                      o);
            EXPECT_EQ(SCO(0),
                      y);
        }
    }

    client_->clear();
    i = 0;
    client_->getEntries(fun);
    EXPECT_EQ(0,
              i);

    client_->getSCORange(oldest,
                         youngest);
    EXPECT_EQ(SCO(0),
              oldest);
    EXPECT_EQ(SCO(0),
              youngest);
}

TEST_P(FailOverCacheClientInterfaceTest, range_performance)
{
    const size_t max =
        yt::System::get_env_with_default("DTL_CLIENT_INTERFACE_TEST_ITERATIONS",
                                         10000ULL);

    yt::wall_timer t;

    for (size_t i = 0; i < max; ++i)
    {
         SCO oldest;
         SCO youngest;
         client_->getSCORange(oldest, youngest);
         EXPECT_EQ(SCO(0),
                   oldest);
         EXPECT_EQ(SCO(0),
                   youngest);
    }

    std::cout << max << " iterations took " << t.elapsed() << " seconds -> " <<
        (max / t.elapsed()) << " IOPS" << std::endl;
}

TEST_P(FailOverCacheClientInterfaceTest, owner_tag)
{
    const OwnerTag owner_tag(2);
    ASSERT_NE(owner_tag_,
              owner_tag);

    const std::vector<uint8_t> buf(default_cluster_size(), 0xff);
    const std::vector<ClusterLocation> locs({ ClusterLocation(1, 0), ClusterLocation(2, 0) });

    for (const auto& loc : locs)
    {
        client_->addEntries({ loc },
                            0,
                            buf.data()).get();
    }

    client_->flush().get();

    std::shared_ptr<yt::AsioServiceManager>
        mgr(VolManager::get()->asio_service_manager());
    std::unique_ptr<failovercache::ClientInterface>
        client(ClientInterface::create(mgr->get_service(0),
                                       mgr->implicit_strand(),
                                       foc_ctx_->config(GetParam().foc_mode()),
                                       wrns_->ns(),
                                       OwnerTag(2),
                                       default_lba_size(),
                                       default_cluster_multiplier(),
                                       boost::chrono::milliseconds(3000),
                                       boost::chrono::milliseconds(3000)));

    if ((protocol_features.t bitand ProtocolFeature::TunnelCapnProto))
    {
        EXPECT_THROW(client_->clear(),
                     std::exception);
        EXPECT_THROW(client_->flush().get(),
                     std::exception);
        EXPECT_THROW(client_->addEntries({ ClusterLocation(3, 0) },
                                         0,
                                         buf.data()).get(),
                     std::exception);
    }

    client->flush().get();

    SCO oldest;
    SCO youngest;
    client_->getSCORange(oldest,
                         youngest);
    EXPECT_EQ(SCO(1),
              oldest);
    EXPECT_EQ(SCO(2),
              youngest);

    size_t count = 0;
    client_->getEntries([&](ClusterLocation loc,
                           Lba,
                           const uint8_t*,
                           size_t)
                        {
                            ++count;
                            EXPECT_TRUE(loc == locs[0] or
                                        loc == locs[1]);
                        });

    EXPECT_EQ(locs.size(),
              count);

    client->removeUpTo(oldest);

    client_->getSCORange(oldest,
                         youngest);
    EXPECT_EQ(SCO(2),
              oldest);
    EXPECT_EQ(SCO(2),
              youngest);

    count = 0;
    client_->getSCOFromFailOver(SCO(2),
                                [&](ClusterLocation loc,
                                    Lba,
                                    const uint8_t*,
                                    size_t)
                                {
                                    ++count;
                                    EXPECT_EQ(locs[1],
                                              loc);
                                });
    EXPECT_EQ(1,
              count);

    client->clear();

    client_->getSCORange(oldest,
                         youngest);
    EXPECT_EQ(SCO(0),
              oldest);
    EXPECT_EQ(SCO(0),
              youngest);
}

namespace
{

const VolumeDriverTestConfig capnp_config =
    VolumeDriverTestConfig().dtl_protocol_features(ProtocolFeatures(ProtocolFeature::TunnelCapnProto));

const VolumeDriverTestConfig backward_config =
    VolumeDriverTestConfig().dtl_protocol_features(ProtocolFeatures());

INSTANTIATE_TEST_CASE_P(FailOverCacheClientInterfaceTests,
                        FailOverCacheClientInterfaceTest,
                        ::testing::Values(capnp_config,
                                          backward_config));
}

}
