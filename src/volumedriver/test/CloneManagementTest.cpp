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

#include "../VolManager.h"
#include "VolManagerTestSetup.h"

namespace volumedriver
{

class CloneManagementTest
    : public VolManagerTestSetup
{
public:
    CloneManagementTest()
        : VolManagerTestSetup("CloneManagementTest")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }

};

TEST_P(CloneManagementTest, noclonesfromsnapshotsnotinbackend)
{
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v = newVolume("volume1",
			  ns1);

    SharedVolumePtr c = 0;
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    ASSERT_TRUE(v != nullptr);
    {
        SCOPED_BLOCK_BACKEND(*v);

        writeToVolume(*v,
                      0,
                      4096,
                      "xyz");
        v->createSnapshot(SnapshotName("snap1"));


        ASSERT_THROW(c = createClone("clone1",
                                     ns2,
                                     ns1,
                                     "snap1"),
                     fungi::IOException);

        ASSERT_FALSE(c);
    }

    waitForThisBackendWrite(*v);


    ASSERT_NO_THROW(c = createClone("clone1",
                                    ns2,
                                    ns1,
                                    "snap1"));

    ASSERT_TRUE(c != nullptr);
    checkVolume(*c,
                0,
                4096,
                "xyz");

}

TEST_P(CloneManagementTest, sad_clone)
{
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns1_ptr->ns();


    //    backend::Namespace ns1;

    SharedVolumePtr v = newVolume("volume1",
                          ns1);


    ASSERT_TRUE(v != nullptr);
    writeToVolume(*v,
                  0,
                  4096,
                  "xyz");
    v->createSnapshot(SnapshotName("snap1"));
    persistXVals(VolumeId("volume1"));

    waitForThisBackendWrite(*v);
    waitForThisBackendWrite(*v);
    const VolumeConfig cfg(v->get_config());

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    v = 0;
    SharedVolumePtr c = 0;

    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    ASSERT_NO_THROW(c = createClone("clone1",
                                    ns2,
                                    ns1,
                                    "snap1"));


    ASSERT_TRUE(c != nullptr);

    restartVolume(cfg);
    sleep(5);

}

TEST_P(CloneManagementTest, recursiveclonelookup)
{
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns1_ptr->ns();


    SharedVolumePtr v = newVolume("volume1",
                          ns1);


    ASSERT_TRUE(v != nullptr);
    writeToVolume(*v,
                  0,
                  4096,
                  "xyz");
    v->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(*v);
    waitForThisBackendWrite(*v);

    SharedVolumePtr c = 0;

    auto ns2_ptr = make_random_namespace();
    const backend::Namespace& clone1ns = ns2_ptr->ns();


    ASSERT_NO_THROW(c = createClone("clone1",
                                    clone1ns,
                                    ns1,
                                    "snap1"));


    ASSERT_TRUE(c != nullptr);
    checkVolume(*v,
                0,
                4096,
                "xyz");

    writeToVolume(*c,
                  8,
                  4096,
                  "abc");


    c->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(*c);
    waitForThisBackendWrite(*c);

    SharedVolumePtr d = 0;

    auto ns3_ptr = make_random_namespace();
    const backend::Namespace& ns3 = ns3_ptr->ns();

    ASSERT_NO_THROW(d = createClone("clone2",
                                    ns3,
                                    clone1ns,
                                   "snap1"));

    ASSERT_TRUE(d != nullptr);
    checkVolume(*d,
                0,
                4096,
                "xyz");

    checkVolume(*d,
                8,
                4096,
                "abc");
}

namespace
{

const VolumeDriverTestConfig a_config =
    VolumeDriverTestConfig().use_cluster_cache(false);

}

INSTANTIATE_TEST_CASE_P(CloneManagementTests,
                        CloneManagementTest,
                        ::testing::Values(a_config));

}

// Local Variables: **
// mode: c++ **
// End: **
