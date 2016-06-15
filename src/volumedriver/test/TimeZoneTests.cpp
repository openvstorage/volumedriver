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

TEST_F(TimeZoneTests, DISABLED_test1)
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
