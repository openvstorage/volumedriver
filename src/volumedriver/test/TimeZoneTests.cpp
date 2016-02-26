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

#include "ExGTest.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include "youtils/Assert.h"

using namespace boost::posix_time;

namespace test
{

using namespace volumedriver;


class TimeZoneTests : public ExGTest
{

protected:
    void
    setUp()
    {
        ASSERT(saved_tz.empty());
        saved_tz = getenv("TZ");
        unsetenv("TZ");
    }


    void
    tearDown()
    {
        setenv("TZ", saved_tz.c_str(), 1);
    }

    void
    set_time_zone(const std::string& zone)
    {
        setenv("TZ", zone.c_str(), 1);
        tzset();
    }

    std::string saved_tz;

};

TEST_F(TimeZoneTests, test1)
{
    auto val = getenv("TZ");
    (void) val;

    set_time_zone("CEST+5");
    ptime t1 = second_clock::local_time();
    ptime t12 = second_clock::universal_time();

    set_time_zone("CEST+3");
    ptime t2 = second_clock::local_time();
    ptime t22 = second_clock::universal_time();
    time_duration duration = t2 - t1;
    EXPECT_TRUE(duration.total_seconds() >= 7200);
    EXPECT_TRUE(duration.total_seconds() <= 7202);
    time_duration duration2 = t22 - t12;
    EXPECT_TRUE(duration2.total_seconds() >= 0);
    EXPECT_TRUE(duration2.total_seconds() <= 2);
}

}

// Local Variables: **
// mode: c++ **
// End: **
