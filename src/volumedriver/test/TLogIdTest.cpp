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

#include "../TLog.h"
#include "../TLogId.h"

#include "ExGTest.h"

#include <boost/lexical_cast.hpp>

#include <youtils/UUID.h>

namespace volumedrivertest
{

using namespace volumedriver;
namespace yt = youtils;

class TLogIdTest
    : public testing::TestWithParam<VolumeDriverTestConfig>
{};

TEST_F(TLogIdTest, basics)
{
    TLogId id;
    EXPECT_NE(id.t,
              yt::UUID::NullUUID());

    EXPECT_EQ(id,
              boost::lexical_cast<TLogId>(id));

    const auto s(boost::lexical_cast<std::string>(id));
    EXPECT_EQ(id,
              boost::lexical_cast<TLogId>(s));

    EXPECT_TRUE(TLog::isTLogString(s));
    EXPECT_FALSE(TLog::isTLogString(s.substr(1)));
    EXPECT_FALSE(TLog::isTLogString(s + "0"));
}

}
