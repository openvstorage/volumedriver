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
