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

#include "ExGTest.h"
#include "../VolumeDriverError.h"

namespace volumedriver
{

struct VolumeDriverErrorTest
    : public VolManagerTestSetup
{
    VolumeDriverErrorTest()
        : VolManagerTestSetup("VolumeDriverErrorTest")
    {}
};

TEST_P(VolumeDriverErrorTest, simple)
{
    ASSERT_EQ(0U,
              event_collector_->size());
    ASSERT_TRUE(boost::none == event_collector_->pop());

    const events::VolumeDriverErrorCode ec(events::VolumeDriverErrorCode::Unknown);
    const VolumeId id("some-volume");
    const std::string msg1("first error");
    const std::string msg2("second error");

    VolumeDriverError::report(ec,
                              msg1,
                              id);

    ASSERT_EQ(1U, event_collector_->size());;

    VolumeDriverError::report(ec,
                              msg2);

    ASSERT_EQ(2U, event_collector_->size());;

    using MaybeEvent = boost::optional<events::Event>;

    {
        const MaybeEvent maybe_ev(event_collector_->pop());
        ASSERT_TRUE(boost::none != maybe_ev);

        ASSERT_TRUE(maybe_ev->HasExtension(events::volumedriver_error));

        const events::VolumeDriverErrorEvent&
            err(maybe_ev->GetExtension(events::volumedriver_error));

        ASSERT_TRUE(err.has_code());
        ASSERT_EQ(ec, err.code());

        ASSERT_TRUE(err.has_info());
        ASSERT_EQ(msg1, err.info());

        ASSERT_TRUE(err.has_volume_name());
        ASSERT_EQ(id.str(), err.volume_name());
    }

    {
        const MaybeEvent maybe_ev(event_collector_->pop());
        ASSERT_TRUE(boost::none != maybe_ev);

        const events::VolumeDriverErrorEvent&
            err(maybe_ev->GetExtension(events::volumedriver_error));

        ASSERT_TRUE(err.has_code());
        ASSERT_EQ(ec, err.code());

        ASSERT_TRUE(err.has_info());
        ASSERT_EQ(msg2, err.info());

        ASSERT_FALSE(err.has_volume_name());
    }

    ASSERT_EQ(0U, event_collector_->size());
}

INSTANTIATE_TEST(VolumeDriverErrorTest);

}

// Local Variables: **
// mode: c++ **
// End: **
