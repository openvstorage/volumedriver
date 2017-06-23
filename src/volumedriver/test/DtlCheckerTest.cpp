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

#include <boost/thread/thread.hpp>

namespace volumedriver
{

class DtlCheckerTest
    : public VolManagerTestSetup
{
public:
    DtlCheckerTest()
        : VolManagerTestSetup(VolManagerTestSetupParameters("DtlCheckerTest")
                              .dtl_check_interval(boost::chrono::seconds(2)))
    {}
};

TEST_P(DtlCheckerTest, auto_recovery)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume("vol1",
                          ns);

    const auto port = get_next_foc_port();
    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                               port,
                                                               GetParam().foc_mode())),
                 std::exception);

    ASSERT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    auto foc_ctx(start_one_foc());
    ASSERT_EQ(port,
              foc_ctx->port());

    boost::this_thread::sleep_for(2 * dtl_check_interval_);

    ASSERT_EQ(VolumeFailOverState::OK_SYNC,
              v->getVolumeFailOverState());
}

INSTANTIATE_TEST_CASE_P(DtlCheckerTests,
                        DtlCheckerTest,
                        ::testing::Values(VolumeDriverTestConfig()));

}
