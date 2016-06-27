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

#include "DtlEnvironment.h"
#include "FailOverCacheTestMain.h"

#include "../DtlServer.h"

#include "../../ClusterLocation.h"
#include "../../FailOverCacheConfig.h"
#include "../../FailOverCacheProxy.h"

#include <stdlib.h>
#include <memory>

#include <gtest/gtest.h>

#include <boost/program_options.hpp>
#include <boost/random/discrete_distribution.hpp>
#include <boost/thread.hpp>

#include <youtils/Logger.h>
#include <youtils/ScopeExit.h>
#include <youtils/System.h>
#include <youtils/TestBase.h>

namespace distributed_transaction_log_test
{

namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace yt = youtils;

using namespace volumedriver;
using namespace std::literals::string_literals;

#define BIND_SCO_PROCESSOR(proc) \
    SCOProcessorFun(boost::bind(&decltype(proc)::operator(), &proc, _1, _2, _3, _4))

class FailOverCacheEntryFactory
{
public:
    FailOverCacheEntryFactory(ClusterSize cluster_size,
                              uint32_t num_clusters,
                              ClusterLocation startLocation =  ClusterLocation(1))
        : cluster_size_(cluster_size),
          num_clusters_(num_clusters),
          cluster_loc(startLocation)
    {}

    FailOverCacheEntry
    operator()(ClusterLocation& next_location,
               const std::string& content = "")
    {
        size_t size = cluster_size_;
        byte* b = new byte[size];
        size_t content_size = content.size();

        if(content_size > 0)
        {
            for(unsigned int i = 0; i < cluster_size_; ++i)
            {
                b[i] = content[i % content_size];
            }
        }

        FailOverCacheEntry ret(cluster_loc,
                               0,
                               b,
                               cluster_size_);
        SCOOffset a = cluster_loc.offset();

        if(a  >= num_clusters_ -1)
        {
            cluster_loc.incrementNumber();
            cluster_loc.offset(0);

        }
        else
        {
            cluster_loc.incrementOffset();
        }
        next_location = cluster_loc;
        return ret;
    }

private:
    const ClusterSize cluster_size_;
    const uint32_t num_clusters_;
    ClusterLocation cluster_loc;
};

class FailOverCacheEntryProcessor
{
public:
    FailOverCacheEntryProcessor(const std::string& content,
                                const ClusterSize cluster_size)
        : sco_count(0)
        , cluster_count(0)
        , content_(content)
        , prev_sconame(0)
        , cluster_size_(cluster_size)
    {}

    void
    operator()(ClusterLocation cl,
               uint64_t lba,
               const byte* buf,
               size_t size)
    {
        LOG_TRACE("Got cli " << cl
                  << ", lba " << lba
                  << ", size " << size);
        cluster_count++;
        SCO sconame = cl.sco();
        if(sconame != prev_sconame)
        {
            ++sco_count;
            prev_sconame = sconame;
        }
        EXPECT_TRUE((uint32_t)size == cluster_size_);

        for(uint32_t i = 0; i < size; ++i)
        {
            size_t mysize = content_.length();
            ASSERT_TRUE(buf[i] == content_[i%mysize]);
        }
    }

    DECLARE_LOGGER("FailOverCacheProcessor");

    unsigned sco_count;
    unsigned cluster_count;
    const std::string content_;
    SCO prev_sconame ;
    const ClusterSize cluster_size_;
};

class FailOverCacheTest
    : public youtilstest::TestBase
{

    virtual void
    SetUp()
    {
        v.reset(new DtlEnvironment(FailOverCacheTestMain::host(),
                                             FailOverCacheTestMain::port(),
                                             FailOverCacheTestMain::transport()));
        v->SetUp();

    }

    virtual void
    TearDown()
    {
        v->TearDown();

        v.reset();
    }

    std::unique_ptr<DtlEnvironment> v;
};

TEST_F(FailOverCacheTest, put_and_retrieve)
{
    const ClusterSize cluster_size(4096);
    const uint32_t num_clusters_per_sco = 32;
    const uint32_t num_scos_to_produce = 20;

    FailOverCacheProxy
        cache(FailOverCacheTestMain::failovercache_config(),
              FailOverCacheTestMain::ns(),
              LBASize(512),
              ClusterMultiplier(8),
              boost::chrono::seconds(8));

    FailOverCacheEntryFactory factory(cluster_size,
                                      num_clusters_per_sco);
    ClusterLocation next_location(1);
    for(uint32_t i = 0; i < num_scos_to_produce; ++i)
    {
        std::vector<FailOverCacheEntry> vec;
        for(uint32_t k = 0; k < num_clusters_per_sco; ++k)
        {
            vec.emplace_back(factory(next_location,
                                     "bart"));
        }
        cache.addEntries(vec);
        auto end = vec.end();
        for (auto it = vec.begin(); it != end; it++)
        {
            delete[] it->buffer_;
        }
    }

    for(int i = 0; i < 8; ++i)
    {
        FailOverCacheEntryProcessor processor("bart",
                                              cluster_size);
        cache.getEntries(BIND_SCO_PROCESSOR(processor));

        EXPECT_EQ(processor.sco_count, num_scos_to_produce);
        EXPECT_EQ(processor.cluster_count, num_clusters_per_sco * num_scos_to_produce);
    }

    cache.clear();
    FailOverCacheEntryProcessor processor("bart",
                                          cluster_size);
    cache.getEntries(BIND_SCO_PROCESSOR(processor));
    EXPECT_EQ(processor.sco_count, 0);
    EXPECT_EQ(processor.cluster_count, 0);
}

TEST_F(FailOverCacheTest, GetSCORange)
{
    const ClusterSize cluster_size(4096);

    const uint32_t num_clusters_per_sco = 32;
    const uint32_t num_clusters_per_vector = 8;

    ASSERT_EQ(0, num_clusters_per_sco % num_clusters_per_vector) << "batches must not cross SCO boundaries";

    const uint32_t num_scos_to_produce = 13;

    FailOverCacheProxy
        cache(FailOverCacheTestMain::failovercache_config(),
              FailOverCacheTestMain::ns(),
              LBASize(512),
              ClusterMultiplier(8),
              boost::chrono::seconds(8));

    FailOverCacheEntryFactory factory(cluster_size,
                                      num_clusters_per_sco);

    ClusterLocation next_location(1);
    bool stopping = false;
    SCO oldest , youngest ;
    cache.getSCORange(oldest,youngest);
    EXPECT_TRUE(oldest.number() <= youngest.number());
    EXPECT_EQ(oldest, SCO(0));
    EXPECT_EQ(youngest,SCO(0));
    while(not stopping)
    {
        std::vector<FailOverCacheEntry> vec;
        for(uint32_t l = 0; l < num_clusters_per_vector; ++l)
        {
            SCONumber next_sco_number = next_location.number();

            if(next_sco_number > num_scos_to_produce)
            {
                stopping = true;
                break;
            }

            if(next_sco_number == 4)
            {
                vec.emplace_back(factory(next_location,
                                         "arne"));
            }
            else
            {
                vec.emplace_back(factory(next_location,
                                         "bart"));
            }
        }
        cache.addEntries(vec);
        auto end = vec.end();
        for (auto it = vec.begin(); it != end; it++)
        {
            delete[] it->buffer_;
        }
    }

    cache.getSCORange(oldest, youngest);
    EXPECT_TRUE(oldest.number() <= youngest.number());
    EXPECT_EQ(oldest, ClusterLocation(1).sco());
    EXPECT_EQ(youngest, ClusterLocation(13).sco());
    SCO l = ClusterLocation(7).sco();
    cache.removeUpTo(SCO(l));
    cache.getSCORange(oldest, youngest);
    EXPECT_TRUE(oldest.number() <= youngest.number());
    EXPECT_EQ(oldest, ClusterLocation(8).sco());
    EXPECT_EQ(youngest, ClusterLocation(13).sco());
    cache.clear();
    cache.getSCORange(oldest, youngest);
    EXPECT_TRUE(oldest.number() <= youngest.number());
    EXPECT_EQ(oldest, SCO());
    EXPECT_EQ(youngest,SCO());
}

TEST_F(FailOverCacheTest, GetOneSCO)
{
    const ClusterSize cluster_size(4096);
    const uint32_t num_clusters_per_sco = 32;
    const uint32_t num_clusters_per_vector = 8;

    ASSERT_EQ(0, num_clusters_per_sco % num_clusters_per_vector) << "batches must not cross SCO boundaries";

    const uint32_t num_scos_to_produce = 13;

    FailOverCacheProxy
        cache(FailOverCacheTestMain::failovercache_config(),
              FailOverCacheTestMain::ns(),
              LBASize(512),
              ClusterMultiplier(8),
              boost::chrono::seconds(8));

    FailOverCacheEntryFactory factory(cluster_size,
                                      num_clusters_per_sco);

    ClusterLocation next_location(1);
    bool stopping = false;
    while(not stopping)
    {
        std::vector<FailOverCacheEntry> vec;
        for(uint32_t l = 0; l < num_clusters_per_vector; ++l)
        {
            SCONumber next_sco_number = next_location.number();

            if(next_sco_number > num_scos_to_produce)
            {
                stopping = true;
                break;
            }

            if(next_sco_number == 4)
            {
                vec.emplace_back(factory(next_location,
                                         "arne"));
            }
            else
            {
                vec.emplace_back(factory(next_location,
                                         "bart"));
            }
        }
        cache.addEntries(vec);
        auto end = vec.end();
        for (auto it = vec.begin(); it != end; it++)
        {
            delete[] it->buffer_;
        }
    }

    {
        FailOverCacheEntryProcessor processor("arne",
                                              cluster_size);

        cache.getSCOFromFailOver(ClusterLocation(4).sco(),
                                 BIND_SCO_PROCESSOR(processor));
        EXPECT_EQ(processor.sco_count, 1);
        EXPECT_EQ(processor.cluster_count, num_clusters_per_sco);
    }

    {
        FailOverCacheEntryProcessor processor("bart",
                                              cluster_size);

        cache.getSCOFromFailOver(ClusterLocation(7).sco(),
                                 BIND_SCO_PROCESSOR(processor));
        EXPECT_EQ(processor.sco_count, 1);
        EXPECT_EQ(processor.cluster_count, num_clusters_per_sco);
    }

    cache.clear();
    FailOverCacheEntryProcessor processor("bart",
                                          cluster_size);
    cache.getEntries(BIND_SCO_PROCESSOR(processor));
    EXPECT_EQ(processor.sco_count, 0);
    EXPECT_EQ(processor.cluster_count, 0);
}

TEST_F(FailOverCacheTest, DISABLED_DoubleRegister)
{
    // Y42 apparantly not a or my problem
    FailOverCacheProxy
        cache1(FailOverCacheTestMain::failovercache_config(),
               FailOverCacheTestMain::ns(),
               LBASize(512),
               ClusterMultiplier(8),
               boost::chrono::seconds(8));
}

struct FailOverCacheOneProcessor
{
    FailOverCacheOneProcessor(const std::string& content,
                              const ClusterSize cluster_size,
                              const SCO sconame,
                              const uint64_t cluster_count)
        : sco_count(0)
        , cluster_num_(cluster_count)
        , content_(content)
        , sconame_(sconame)
        , cluster_size_(cluster_size)
        , cluster_count_(cluster_count)
    {}

    ~FailOverCacheOneProcessor()
    {
        EXPECT_EQ(0, cluster_count_);
    }

    void
    operator()(ClusterLocation cl,
               uint64_t lba,
               const byte* buf,
               size_t size)
    {
        LOG_TRACE("Got cli " << cl
                  << ", lba " << lba
                  << ", size " << size);
        --cluster_count_;
        ASSERT_TRUE(cluster_count_ >= 0);

        SCO sconame = cl.sco();
        ASSERT_TRUE(sconame_ == sconame);
        ASSERT_TRUE((uint32_t)size == cluster_size_);

        for(uint32_t i = 0; i < size; ++i)
        {
            size_t size = content_.length();
            ASSERT_TRUE(buf[i] == content_[i%size]);
        }
    }

    DECLARE_LOGGER("FailOverCacheOneProcessor");
    unsigned sco_count;
    const uint32_t cluster_num_;
    const std::string content_;
    SCO sconame_ ;
    const ClusterSize cluster_size_;
    int32_t cluster_count_;
};

class FailOverCacheTestThread
{
public:
    explicit FailOverCacheTestThread(const std::string& content,
                                     const unsigned test_size = 100000)
        : content_(content)
        , cache_(FailOverCacheTestMain::failovercache_config(),
                 backend::Namespace(content),
                 LBASize(512),
                 ClusterMultiplier(8),
                 boost::chrono::seconds(30))
        , factory_(cluster_size_,
                   num_clusters_per_sco_)
        , next_location_(1)
        , cluster_count_(0)
        , test_size_(test_size)
    {
        EXPECT_EQ(0, num_clusters_per_sco_ % num_clusters_per_vector_);
    }

    DECLARE_LOGGER("FailOverCacheTestThread");

    void operator()()
    {
        unsigned i = 0;
        double probabilities[] = {
            0.005, // Chance for flushing and checking
            0.045, // Chance for checking getEntries
            0.025, // Chance for getSCORange
            0.025, // chance for checking clusters in sco.
            0.9 // just flush
        };

        boost::random::discrete_distribution<> dist(probabilities);
        while(i++ < test_size_)
        {
            LOG_NOTIFY("run " << i << " of " << test_size_ << " for " << content_);

            std::vector<FailOverCacheEntry> vec;
            latestSCOOnFailOver = next_location_;
            // Fill her up
            for(uint32_t l = 0; l < num_clusters_per_vector_; ++l)
            {
                cluster_count_++;
                if(l == num_clusters_per_vector_ - 1)
                {
                    latestSCOOnFailOver = next_location_;
                }
                vec.emplace_back(factory_(next_location_,
                                          FailOverCacheTestMain::ns().str()));
            }

            cache_.addEntries(vec);
            auto end = vec.end();
            for (auto it = vec.begin(); it != end; it++)
            {
                delete[] it->buffer_;
            }
            switch(dist(gen_))
            {
            case 0:
                {
                    LOG_INFO("Test 0");

                    cache_.clear();
                    cluster_count_ = 0;
                    latestSCONotOnFailOver = latestSCOOnFailOver;

                    FailOverCacheEntryProcessor processor(FailOverCacheTestMain::ns().str(),
                                                          cluster_size_);
                    cache_.getEntries(BIND_SCO_PROCESSOR(processor));
                    EXPECT_EQ(processor.sco_count, 0);
                    EXPECT_EQ(processor.cluster_count, cluster_count_);
                }
                break;
            case 1:
                {
                    LOG_INFO("Test 1");

                    FailOverCacheEntryProcessor processor(FailOverCacheTestMain::ns().str(),
                                                          cluster_size_);
                    cache_.getEntries(BIND_SCO_PROCESSOR(processor));
                    EXPECT_EQ(processor.cluster_count, cluster_count_);
                }
                break;
            case 2:
                {
                    LOG_INFO("Test 2");
                    SCO oldest;
                    SCO youngest;
                    cache_.getSCORange(oldest, youngest);
                    EXPECT_TRUE(oldest.number() <= youngest.number());
                    ClusterLocation next = latestSCONotOnFailOver;
                    next.incrementNumber(1);

                    if(latestSCOOnFailOver.number() > latestSCOOnFailOver.number())
                    {
                        EXPECT_TRUE(latestSCONotOnFailOver.sco() == oldest or
                                    next.sco() == oldest);
                        EXPECT_TRUE(latestSCOOnFailOver.sco() == youngest);
                    }
                }
                break;
            case 3:
                if(latestSCOOnFailOver.number() > latestSCONotOnFailOver.number())
                {
                    LOG_INFO("Test 3");
                    double a = drand48();
                    // If last sco not on failover has offset 32 we have a
                    // problem
                    SCONumber num1 = latestSCONotOnFailOver.number();
                    SCONumber num2 = latestSCOOnFailOver.number();
                    SCONumber num3 = std::max((int)(num1 + a * (num2 - num1)), 1);
                    ASSERT(num1 <= num3 and
                           num3 <= num2);
                    SCO sconame = ClusterLocation(num3).sco();
                    uint32_t clusters_in_sco = num_clusters_per_sco_;
                    if(num2 == num1)
                    {
                        clusters_in_sco = latestSCOOnFailOver.offset() - latestSCONotOnFailOver.offset();
                    }
                    if(num3 == num1)
                    {
                        clusters_in_sco = num_clusters_per_sco_ - latestSCONotOnFailOver.offset() -1;
                    }
                    if(num3 == num2)
                    {
                        clusters_in_sco = latestSCOOnFailOver.offset() + 1;
                    }

                    FailOverCacheOneProcessor proc(FailOverCacheTestMain::ns().str(),
                                                   cluster_size_,
                                                   sconame,
                                                   clusters_in_sco);

                    cache_.getSCOFromFailOver(sconame,
                                              BIND_SCO_PROCESSOR(proc));
                }
                break;
            case 4:
                LOG_INFO("Just flushing");

                cache_.flush();
                break;
            default:
            LOG_FATAL("How did you get here");
            }
        }
        LOG_INFO("Exiting test for " << content_);
    }

    const std::string content_;

    FailOverCacheProxy cache_;
    FailOverCacheEntryFactory factory_;

    static const ClusterSize cluster_size_;
    static const uint32_t num_clusters_per_sco_ = 32;
    static const uint32_t num_clusters_per_vector_ = 8;

    ClusterLocation latestSCONotOnFailOver;
    ClusterLocation latestSCOOnFailOver;

    ClusterLocation next_location_;
    uint32_t cluster_count_;
    const unsigned test_size_;

    static double probabilities[];
    boost::random::mt19937 gen_;
};

const ClusterSize
FailOverCacheTestThread::cluster_size_(4096);

TEST_F(FailOverCacheTest, Stress)
{
    std::vector<std::unique_ptr<FailOverCacheTestThread>> test_threads;
    std::vector<boost::thread> threads;

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         for (auto& t : threads)
                                         {
                                             t.join();
                                         }
                                     }));

    const size_t num_clients =
        yt::System::get_env_with_default("FAILOVERCACHE_STRESS_TEST_NUM_CLIENTS",
                                         16ULL);
    const size_t test_size =
        yt::System::get_env_with_default("FAILOVERCACHE_STRESS_TEST_SIZE",
                                         1ULL << 12);

    for(unsigned i = 0; i < num_clients; ++i)
    {
        const std::string content("namespace-" + boost::lexical_cast<std::string>(i));
        auto t(std::make_unique<FailOverCacheTestThread>(content,
                                                         test_size));
        test_threads.emplace_back(std::move(t));
        threads.emplace_back(boost::thread(boost::ref(*test_threads[i])));
    }
}

// https://github.com/openvstorage/volumedriver/issues/19 :
// The corking mechanism lead to too many clusters being queued up on the sender
// (server side), which eventually complained with std::bad_alloc when trying to
// send yet another one.
TEST_F(FailOverCacheTest, get_entries_xxl)
{
    const LBASize lba_size(512);
    const ClusterMultiplier cmult(8);
    const ClusterSize csize(lba_size * cmult);

    FailOverCacheProxy
        cache(FailOverCacheTestMain::failovercache_config(),
              FailOverCacheTestMain::ns(),
              lba_size,
              cmult,
              boost::chrono::seconds(8));

    const SCOMultiplier smult(4096);

    FailOverCacheEntryFactory factory(csize,
                                      smult);

    const size_t test_size = 2ULL << 30;
    const size_t count = test_size / csize;

    const std::vector<uint8_t> buf(csize);

    for (size_t i = 0; i < count; ++i)
    {
        ClusterLocation loc;
        FailOverCacheEntry e(factory(loc));
        ASSERT_EQ(e.size_,
                  buf.size());

        delete[] e.buffer_;
        e.buffer_ = buf.data();

        cache.addEntries({std::move(e)});
    }

    size_t seen = 0;
    cache.getEntries([&](ClusterLocation,
                         uint64_t,
                         const uint8_t*,
                         size_t)
                     {
                         ++seen;
                     });

    EXPECT_EQ(count,
              seen);
}

}
