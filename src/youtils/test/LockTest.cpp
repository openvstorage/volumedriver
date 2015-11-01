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
