// Copyright (C) 2017 iNuron NV
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

#include "../XMLRPCStructs.h"

#include <gtest/gtest.h>

namespace volumedriverfstest
{

using namespace volumedriverfs;

struct XMLRPCStructsTest
    : public testing::Test
{
    template<typename T>
    static void
    test_serialization(const T& t)
    {
        std::string s(t.str());
        T u;
        u.set_from_str(s);
        EXPECT_EQ(t, u);
    }
};

// https://github.com/openvstorage/volumedriver/issues/264
// overlaps with filesystem-python-client/test/fs_python_client_test.py
// but this one makes debugging more convenient.
TEST_F(XMLRPCStructsTest, snapshot_info_serialization)
{
    const XMLRPCSnapshotInfo sinfo("foo",
                                   "bar",
                                   "baz",
                                   12345,
                                   true,
                                   "");
    test_serialization(sinfo);
}

}
