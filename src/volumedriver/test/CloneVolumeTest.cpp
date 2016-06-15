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

    const SnapshotName snap1("snap1");
    v->createSnapshot(snap1);
    checkCurrentBackendSize(*v);
    waitForThisBackendWrite(*v);

    SharedVolumePtr c1 = 0;
    VolumeId c1ID("clone1");
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    c1 = createClone(c1ID,
                     ns2,
                     ns1,
                     snap1);
    checkCurrentBackendSize(*c1);
    writeToVolume(*c1,
                  4096,
                  4096,
                  "x");

    const SnapshotName snap2("snap2");
    c1->createSnapshot(snap2);

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

    c1->restoreSnapshot(snap2);

    checkVolume(*c1, 0, 4096, "a");
    checkVolume(*c1, 4096, 4096, "x");
    checkCurrentBackendSize(*c1);
}

INSTANTIATE_TEST(CloneVolumeTest);

}

// Local Variables: **
// mode: c++ **
// End: **
