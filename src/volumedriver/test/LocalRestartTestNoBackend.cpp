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

#include <boost/foreach.hpp>

#include "../VolManager.h"
#include "../DataStoreNG.h"
#include "../TLogWriter.h"
#include "../MetaDataStoreInterface.h"

namespace volumedriver
{

class LocalRestartTestNoBackend
    : public VolManagerTestSetup
{
public:
    LocalRestartTestNoBackend()
        : VolManagerTestSetup(VolManagerTestSetupParameters("LocalRestartTest")
                              .backend_threads(0))
    {}
};

TEST_P(LocalRestartTestNoBackend, restartWithSnapshot1)
{
    VolumeId vid1("volume1");
    // const backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(vid1,
                           ns1);
    writeToVolume(*v1,
                  0,
                  v1->getClusterSize(),
                  "kristafke");
    createSnapshot(*v1, SnapshotName("snapshot1"));

    destroyVolume(v1,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    v1 = nullptr;
    ASSERT_FALSE(v1 = getVolume(vid1));
    ASSERT_NO_THROW(v1 = localRestart(ns1));
    ASSERT_TRUE(getVolume(vid1) != nullptr);
    checkVolume(*v1, 0, v1->getClusterSize(), "kristafke");
    EXPECT_FALSE(v1->isSyncedToBackendUpTo(SnapshotName("snapshot1")));
}

INSTANTIATE_TEST(LocalRestartTestNoBackend);
}

// Local Variables: **
// mode: c++ **
// End: **
