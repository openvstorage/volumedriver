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

#include "../InitializedParam.h"
#include "../TestBase.h"
#include "../VolumeDriverComponent.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

namespace youtilstest
{

namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace yt = youtils;
namespace ytt = youtilstest;

class VolumeDriverComponentTest
    : public ytt::TestBase
{};

typedef boost::archive::binary_iarchive iarchive_type;
typedef boost::archive::binary_oarchive oarchive_type;

TEST_F(VolumeDriverComponentTest, serialize_update_report)
{
    yt::UpdateReport urep1;
    urep1.update("foo", "old_foo", "new_foo");
    urep1.no_update("bar", "old_bar", "old_bar");

    std::stringstream ss;

    {
        oarchive_type oa(ss);
        oa << urep1;
    }

    yt::UpdateReport urep2;

    {
        iarchive_type ia(ss);
        ia >> urep2;
    }

    EXPECT_TRUE(urep1.getUpdates() == urep2.getUpdates());
    EXPECT_TRUE(urep1.getNoUpdates() == urep2.getNoUpdates());
}

TEST_F(VolumeDriverComponentTest, serialize_configuration_report)
{
    yt::ConfigurationReport crep1;
    crep1.emplace_back(yt::ConfigurationProblem("foo", "bar", "could not frob bar/foo"));

    std::stringstream ss;

    {
        oarchive_type oa(ss);
        oa << crep1;
    }

    yt::ConfigurationReport crep2;

    {
        iarchive_type ia(ss);
        ia >> crep2;
    }

    EXPECT_TRUE(crep1 == crep2);
}

}
