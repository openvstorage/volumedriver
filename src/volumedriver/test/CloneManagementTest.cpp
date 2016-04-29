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

    const SnapshotName snap("snap1");

    {
        SCOPED_BLOCK_BACKEND(*v);

        writeToVolume(*v,
                      0,
                      4096,
                      "xyz");
        v->createSnapshot(snap);


        ASSERT_THROW(c = createClone("clone1",
                                     ns2,
                                     ns1,
                                     snap),
                     fungi::IOException);

        ASSERT_FALSE(c);
    }

    waitForThisBackendWrite(*v);

    ASSERT_NO_THROW(c = createClone("clone1",
                                    ns2,
                                    ns1,
                                    snap));

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

    const SnapshotName snap("snap1");

    v->createSnapshot(snap);
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
                                    snap));

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

    const SnapshotName snap("snap1");
    v->createSnapshot(snap);
    waitForThisBackendWrite(*v);
    waitForThisBackendWrite(*v);

    SharedVolumePtr c = 0;

    auto ns2_ptr = make_random_namespace();
    const backend::Namespace& clone1ns = ns2_ptr->ns();


    ASSERT_NO_THROW(c = createClone("clone1",
                                    clone1ns,
                                    ns1,
                                    snap));


    ASSERT_TRUE(c != nullptr);
    checkVolume(*v,
                0,
                4096,
                "xyz");

    writeToVolume(*c,
                  8,
                  4096,
                  "abc");


    c->createSnapshot(snap);
    waitForThisBackendWrite(*c);
    waitForThisBackendWrite(*c);

    SharedVolumePtr d = 0;

    auto ns3_ptr = make_random_namespace();
    const backend::Namespace& ns3 = ns3_ptr->ns();

    ASSERT_NO_THROW(d = createClone("clone2",
                                    ns3,
                                    clone1ns,
                                    snap));

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
