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

#include "../VolumeConfig.h"
#include "ExGTest.h"

#include <youtils/EnumUtils.h>

namespace volumedrivertest
{
using namespace volumedriver;
namespace yt = youtils;

class StreamEnumTest : public testing::TestWithParam<VolumeDriverTestConfig>
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
