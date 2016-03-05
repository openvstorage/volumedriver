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
        : VolManagerTestSetup("LocalRestartTest",
                              UseFawltyMDStores::F,
                              UseFawltyTLogStores::F,
                              UseFawltyDataStores::F,
                              0)
    {}
};

TEST_P(LocalRestartTestNoBackend, restartWithSnapshot1)
{
    VolumeId vid1("volume1");
    // const backend::Namespace ns1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    Volume* v1 = newVolume(vid1,
                           ns1);
    writeToVolume(v1,
                  0,
                  v1->getClusterSize(),
                  "kristafke");
    createSnapshot(v1, SnapshotName("snapshot1"));


    destroyVolume(v1,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    v1 = 0;
    ASSERT_FALSE(v1 = getVolume(vid1));
    ASSERT_NO_THROW(v1 = localRestart(ns1));
    ASSERT_TRUE(getVolume(vid1));
    checkVolume(v1, 0, v1->getClusterSize(), "kristafke");
    EXPECT_FALSE(v1->isSyncedToBackendUpTo(SnapshotName("snapshot1")));
}

INSTANTIATE_TEST(LocalRestartTestNoBackend);
}

// Local Variables: **
// mode: c++ **
// End: **
