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

#include "../Api.h"

#include <boost/chrono.hpp>

namespace volumedrivertest
{

using namespace volumedriver;
namespace bc = boost::chrono;

class FencingTest
    : public VolManagerTestSetup
{
public:
    FencingTest()
        : VolManagerTestSetup("FencingTest")
    {}
};

TEST_P(FencingTest, reconfigure_volume)
{
    auto rns(make_random_namespace());
    SharedVolumePtr v = newVolume(*rns);
    const SCOMultiplier sco_mult(4096);
    ASSERT_NE(sco_mult,
              v->getSCOMultiplier());

    claim_namespace(rns->ns(),
                    new_owner_tag());

    fungi::ScopedLock l(api::getManagementMutex());

    EXPECT_THROW(v->setSCOMultiplier(sco_mult),
                 std::exception);
    EXPECT_TRUE(v->is_halted());
}

TEST_P(FencingTest, reconfigure_dtl)
{
    auto rns(make_random_namespace());
    SharedVolumePtr v = newVolume(*rns);

    claim_namespace(rns->ns(),
                    new_owner_tag());

    auto foc_ctx(start_one_foc());
    EXPECT_THROW(v->setFailOverCacheConfig(foc_ctx->config(FailOverCacheMode::Asynchronous)),
                 std::exception);
    EXPECT_TRUE(v->is_halted());
}

TEST_P(FencingTest, write)
{
    auto rns(make_random_namespace());
    SharedVolumePtr v = newVolume(*rns);

    const size_t wsize = 64UL << 10;
    const std::string pattern1("before");
    writeToVolume(*v,
                  0,
                  wsize,
                  pattern1);

    v->scheduleBackendSync();
    waitForThisBackendWrite(*v);

    const VolumeConfig cfg(v->get_config());

    claim_namespace(rns->ns(),
                    new_owner_tag());

    const std::string pattern2("after");
    writeToVolume(*v,
                  0,
                  wsize,
                  pattern2);

    v->scheduleBackendSync();

    using Clock = bc::steady_clock;
    const Clock::time_point deadline = Clock::now() + bc::seconds(60);

    while (not v->is_halted() and Clock::now() < deadline)
    {
        boost::this_thread::sleep_for(bc::milliseconds(250));
    }

    ASSERT_TRUE(v->is_halted()) << "volume should be halted by now";

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    restartVolume(cfg);
    v = getVolume(cfg.id_);
    ASSERT_TRUE(v != nullptr);

    checkVolume(*v,
                0,
                wsize,
                pattern1);
}

INSTANTIATE_TEST_CASE_P(FencingTests,
                        FencingTest,
                        ::testing::Values(VolManagerTestSetup::default_test_config()));

}
