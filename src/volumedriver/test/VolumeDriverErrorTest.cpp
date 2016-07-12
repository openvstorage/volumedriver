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

#include "VolumeDriverTestConfig.h"
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
