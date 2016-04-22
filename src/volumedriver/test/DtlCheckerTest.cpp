// Copyright 2016 iNuron NV
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
                 fungi::IOException);

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
