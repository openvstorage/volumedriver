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
#include "../Mutex.h"

namespace youtilstest
{
//using namespace youtils;

class LockTest : public TestBase
{
};


TEST_F(LockTest, test1)
{
    fungi::Mutex m("ello");
    m.lock();
    EXPECT_THROW(fungi::ScopedLock(m, true),fungi::IOException);

    m.unlock();

    EXPECT_NO_THROW(fungi::ScopedLock(m, true));
    //    EXPECT_TRUE(not m.isLocked());

}
}

// Local Variables: **
// bvirtual-targets: ("bin/vd_test") **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// End: **
