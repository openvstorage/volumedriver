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
    : public ExGTest
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
