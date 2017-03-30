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
#include <boost/thread.hpp>
#include "../Api.h"
#include <youtils/wall_timer.h>
#include <youtils/SourceOfUncertainty.h>
#include <youtils/System.h>

namespace volumedrivertest
{

namespace yt = youtils;
using namespace volumedriver;

unsigned init = 0;

struct VolumeReader
{
    VolumeReader(SharedVolumePtr v,
                 uint64_t read_times,
                 uint64_t max_lba)
        : v_(v)
        , len_(4096)
        , buf_(len_)
        , read_times_(read_times)

        , max_lba_(max_lba)
    {
        ASSERT(len_ == 4096);
    }

    ~VolumeReader()
    {}

    DECLARE_LOGGER("VolumeReader");

    void
    operator()()
    {
        boost::random::uniform_int_distribution<uint64_t> dist(0, max_lba_);
        while(not go_)
        {
        }
        for(uint64_t i = 0; i < read_times_; ++i)
        {
            const uint64_t lba = dist(gen_);
            api::Read(v_,
                      Lba(lba*8),
                      buf_.data(),
                      len_);
        }

    }

    SharedVolumePtr v_;
    const uint64_t len_;
    std::vector<uint8_t> buf_;
    const uint64_t read_times_;
    static bool go_;

    uint64_t max_lba_;

    boost::random::mt19937 gen_ {++init};


    static void go(bool go)
    {
        go_ = go;
    }

};

bool
VolumeReader::go_ = false;
// This test won't run as it stands.
// 1) you need to be able to put sleeps in the Local Backend to make sure the sco cache get's drained
// fast enough.
// 2) You don't want these timeouts when filling up the Backend only when reading -> I commented out the
// nanosleep in Local_Connection to run this test.

class ReadParallelismTest
    : public VolManagerTestSetup
{
public:
    ReadParallelismTest()
        : VolManagerTestSetup(VolManagerTestSetupParameters("ReadParallelismTest")
                              .sco_cache_mp1_size("10MiB")
                              .sco_cache_mp2_size("10MiB")
                              .sco_cache_trigger_gap("8MiB")
                              .sco_cache_backoff_gap("9MiB")
                              .sco_cache_cleanup_interval(1))
    {}

    void
    SetUp()
    {
        const uint64_t backend_delay_nanoseconds = 100 * 1000 * 1000;
        youtils::System::set_env("LOCAL_BACKEND_DELAY_NSEC",
                                 backend_delay_nanoseconds,
                                 true);

        VolManagerTestSetup::SetUp();

    }

    void
    TearDown()
    {
        VolManagerTestSetup::TearDown();
        youtils::System::unset_env("LOCAL_BACKEND_DELAY_NSEC");

    }

    uint64_t
    fill_with_scos(SharedVolumePtr v,
                   uint64_t times_sco_cache)
    {
        // This size is related to the scocache sizes defined above!!!!
        const uint64_t sco_cache_size = yt::DimensionedValue("40MiB").getBytes();

        const uint64_t sco_size = v->getSCOSize();

        const uint64_t num_scos = (sco_cache_size / sco_size) * times_sco_cache;
        const uint64_t sco_multiplier = v->getSCOMultiplier();

        for(unsigned i = 0; i < num_scos; ++i)
        {
            VERIFY(4096*sco_multiplier == sco_size);
            writeToVolume(*v,
                          Lba(i*8),
                          4096*sco_multiplier,
                          "kutmetperen");
        }
        return num_scos;
    }

};

TEST_P(ReadParallelismTest, DISABLED_single_read)
{

    //     set environment LOCAL_BACKEND_DELAY_NSEC 100000000
    // mutiple of 2
    //     const uint64_t max_threads = 8;

    const uint64_t test_size = 64 * 8;
    const std::string id1("id1");
    const backend::Namespace ns1;

    SharedVolumePtr v = VolManagerTestSetup::newVolume(id1,
                                                       ns1);

    uint64_t max_lba = fill_with_scos(v,
                                      100);


    ASSERT_NO_THROW(v->createSnapshot(SnapshotName("snap1")));
    waitForThisBackendWrite(*v);
    youtils::wall_timer wt;

    for(int i = 1; i <= 8; i*=2)
    {
        VolumeReader::go(false);

        uint64_t read_size = test_size / i;
        std::vector<VolumeReader> readers;
        readers.reserve(i);

        std::vector<boost::thread> threads;
        threads.reserve(i);

        uint64_t total_to_read = 0;

        for(int k = 0; k < i; ++k)
        {
            readers.emplace_back(v, read_size, max_lba);
            total_to_read += read_size;
            threads.emplace_back(boost::ref(readers.back()));
        }
        VERIFY(total_to_read == test_size);
        wt.restart();
        VolumeReader::go(true);
        for(boost::thread& t: threads)
        {
            t.join();
        }
        std::cout << "Elapsed time for " << i
                  << " threads doing " << test_size << " reads " << wt.elapsed() << std::endl;
    }
}

namespace
{
const VolumeDriverTestConfig thisConfig =
    VolumeDriverTestConfig().use_cluster_cache(false);
}

INSTANTIATE_TEST_CASE_P(ReadParallelismTests,
                        ReadParallelismTest,
                        ::testing::Values(thisConfig));


}
