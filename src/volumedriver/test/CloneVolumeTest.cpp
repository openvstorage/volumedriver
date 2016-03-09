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

#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/filesystem/fstream.hpp>
#include <boost/scope_exit.hpp>

 #include "../VolumeConfig.h"
#include "../VolManager.h"

#include "VolManagerTestSetup.h"
#include <backend/BackendInterface.h>

namespace volumedrivertest
{
using namespace volumedriver;

class CloneVolumeTest
    : public VolManagerTestSetup
{
public:
    CloneVolumeTest()
        : VolManagerTestSetup("CloneVolumeTest")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }
};

TEST_P(CloneVolumeTest, requireParentSnapshotOnNormalClone)
{
    SharedVolumePtr v = 0;
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns1_ptr->ns();


    v = newVolume(VolumeId("volume1"),
                  ns1);
    ASSERT_TRUE(v != nullptr);

    v->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(*v);
    VolumeId c1ID("clone1");
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    EXPECT_THROW(createClone(c1ID,
                             ns2,
                             ns1,
                             boost::none),
                 VolManager::VolumeNotTemplatedButNoSnapshotSpecifiedException);
}

TEST_P(CloneVolumeTest, test1)
{
    SharedVolumePtr v = 0;
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns1_ptr->ns();

    //    backend::Namespace ns1;

    v = newVolume(VolumeId("volume1"),
                  ns1);
    ASSERT_TRUE(v != nullptr);
    writeToVolume(*v,
                  0,
                  4096,
                  "a");

    v->createSnapshot(SnapshotName("snap1"));
    checkCurrentBackendSize(*v);
    waitForThisBackendWrite(*v);

    SharedVolumePtr c1 = 0;
    VolumeId c1ID("clone1");
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    c1 = createClone(c1ID,
                     ns2,
                     ns1,
                     "snap1");
    checkCurrentBackendSize(*c1);
    writeToVolume(*c1,
                  4096,
                  4096,
                  "x");

    c1->createSnapshot(SnapshotName("snap2"));

    waitForThisBackendWrite(*c1);
    writeToVolume(*c1,
                  4096,
                  4096,
                  "b");

    writeToVolume(*c1,
                  0,
                  4096,
                  "c");
    checkCurrentBackendSize(*c1);

    waitForThisBackendWrite(*c1);

    c1->restoreSnapshot(SnapshotName("snap2"));

    checkVolume(*c1, 0, 4096, "a");
    checkVolume(*c1, 4096, 4096, "x");
    checkCurrentBackendSize(*c1);
}

INSTANTIATE_TEST(CloneVolumeTest);

}

// Local Variables: **
// mode: c++ **
// End: **
