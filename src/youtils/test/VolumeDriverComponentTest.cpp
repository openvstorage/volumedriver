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
