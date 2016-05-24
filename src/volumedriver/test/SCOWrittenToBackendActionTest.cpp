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


#include "../SCOWrittenToBackendAction.h"

#include "VolManagerTestSetup.h"

namespace volumedrivertest
{

using namespace volumedriver;

class SCOWrittenToBackendActionTest
    : public VolManagerTestSetup
{
protected:
    SCOWrittenToBackendActionTest()
        : VolManagerTestSetup("SCOWrittenToBackendActionTest")
    {}
};

TEST_P(SCOWrittenToBackendActionTest, enum_streaming)
{
    auto check([](const SCOWrittenToBackendAction a)
               {
                   std::stringstream ss;
                   ss << a;
                   EXPECT_TRUE(ss.good());

                   SCOWrittenToBackendAction b = SCOWrittenToBackendAction::SetDisposable;
                   ss >> b;

                   EXPECT_FALSE(ss.bad());
                   EXPECT_FALSE(ss.fail());
                   EXPECT_TRUE(ss.eof());

                   EXPECT_EQ(a, b);
               });

    check(SCOWrittenToBackendAction::SetDisposable);
    check(SCOWrittenToBackendAction::SetDisposableAndPurgeFromPageCache);
    check(SCOWrittenToBackendAction::PurgeFromSCOCache);
}

TEST_P(SCOWrittenToBackendActionTest, enum_streaming_failure)
{
    std::stringstream ss("fail");
    SCOWrittenToBackendAction a = SCOWrittenToBackendAction::SetDisposable;
    ss >> a;

    EXPECT_TRUE(ss.fail());
    EXPECT_EQ(SCOWrittenToBackendAction::SetDisposable,
              a);
}

INSTANTIATE_TEST_CASE_P(SCOWrittenToBackendActionTests,
                        SCOWrittenToBackendActionTest,
                        ::testing::Values(VolManagerTestSetup::default_test_config()));

}
