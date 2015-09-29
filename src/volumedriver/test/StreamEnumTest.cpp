// Copyright 2015 Open vStorage NV
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

#include "../VolumeConfig.h"
#include "ExGTest.h"

#include <youtils/EnumUtils.h>

namespace volumedrivertest
{
using namespace volumedriver;
namespace yt = youtils;

class StreamEnumTest : public ExGTest
{
protected:
    template<typename T>
    void
    test_enum_stream(std::vector<std::pair<T, std::string> >& enum_values)
    {
        for (const auto& val : enum_values)
        {
            std::stringstream ss1;
            ss1 << val.first;
            std::string print_representation = ss1.str();
            ASSERT_FALSE(print_representation.empty());
            ASSERT_TRUE(print_representation == val.second);
            std::cout << print_representation << std::endl;
            std::stringstream ss2(print_representation);
            T new_val;
            ss2 >> new_val;
            ASSERT_TRUE(new_val == val.first);
        }
        for(unsigned i = 0; i < yt::EnumTraits<T>::size; ++i)
        {
            T a1 = static_cast<T>(i);
            std::stringstream ss(yt::EnumTraits<T>::strings[i]);
            T a2;
            ss >> a2;
            ASSERT_TRUE(a1 == a2);
        }
    }
};

TEST_F(StreamEnumTest, VolumeRole)
{
    std::vector<std::pair<VolumeConfig::WanBackupVolumeRole, std::string> > volume_roles =
        {
            std::make_pair(VolumeConfig::WanBackupVolumeRole::WanBackupNormal, "Normal"),
            std::make_pair(VolumeConfig::WanBackupVolumeRole::WanBackupBase, "BackupBase"),
            std::make_pair(VolumeConfig::WanBackupVolumeRole::WanBackupIncremental, "BackupIncremental")
        };
    test_enum_stream(volume_roles);
}
}

// Local Variables: **
// mode: c++ **
// End: **
