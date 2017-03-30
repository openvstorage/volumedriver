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

#include "VolManagerTestSetup.h"

namespace volumedriver
{
class ToolCutTest
    : public VolManagerTestSetup
{
public:
    ToolCutTest()
        : VolManagerTestSetup("ToolCutTest")
    {}
};

TEST_P(ToolCutTest, DISABLED_test1)
{
    VolumeId vid("volume");
    backend::Namespace ns;
    SharedVolumePtr v = newVolume(vid,
                          ns);

    const std::string pattern("immanuel");
    sleep(1000);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(ToolCutTest, DISABLED_test2)
{
    VolumeId vid("volume");
    backend::Namespace ns;
    SharedVolumePtr v = newVolume(vid,
                          ns);

    const SnapshotName snapshot("snapshot");
    createSnapshot(*v,
                   snapshot);

    waitForThisBackendWrite(*v);

    SharedVolumePtr c = createClone("clone",
                            backend::Namespace(),
                            ns,
                            snapshot);

    const std::string pattern("immanuel");
    sleep(1000);
    destroyVolume(c,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);


}

TEST_P(ToolCutTest, DISABLED_toolcut_create_volume)
{
    backend::Namespace ns1;

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(*v, Lba(0), 4096,"immanuel");

    const SnapshotName snap1("snap1");

    v->createSnapshot(snap1);

    waitForThisBackendWrite(*v);
    sleep(100000);

}

INSTANTIATE_TEST(ToolCutTest);

}
// Local Variables: **
// mode: c++ **
// End: **
