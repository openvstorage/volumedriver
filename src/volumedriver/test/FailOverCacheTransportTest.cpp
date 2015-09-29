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

#include "ExGTest.h"

#include <../FailOverCacheTransport.h>

#include <boost/lexical_cast.hpp>

namespace volumedrivertest
{
using namespace volumedriver;

class FailOverCacheTransportTest
    : public ExGTest
{};

TEST_F(FailOverCacheTransportTest, lexical_cast)
{
    auto fun([&](FailOverCacheTransport t)
             {
                 EXPECT_EQ(t,
                           boost::lexical_cast<FailOverCacheTransport>(boost::lexical_cast<std::string>(t)));
             });

    fun(FailOverCacheTransport::TCP);
    fun(FailOverCacheTransport::RSocket);
}

}
