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

#include "../TestBase.h"
#include "../SpinLock.h"

namespace youtilstest
{

class SpinLockTest : public TestBase
{
protected:
    SpinLockTest()
        : spinLock_()
    {}

    fungi::SpinLock spinLock_;
};

TEST_F(SpinLockTest, basics)
{
    spinLock_.lock();
    EXPECT_FALSE(spinLock_.tryLock());

    spinLock_.unlock();
    EXPECT_TRUE(spinLock_.tryLock());

    spinLock_.unlock();
}

TEST_F(SpinLockTest, assertLocked)
{
    spinLock_.lock();
    spinLock_.assertLocked();
    spinLock_.unlock();
}
}

// Local Variables: **
// bvirtual-targets: ("bin/vd_test") **
// mode: c++ **
// End: **
