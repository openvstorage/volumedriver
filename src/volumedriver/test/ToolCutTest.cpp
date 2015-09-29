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
    Volume* v = newVolume(vid,
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
    Volume* v = newVolume(vid,
                          ns);

    createSnapshot(v,"snapshot");
    waitForThisBackendWrite(v);

    Volume* c = createClone("clone",
                            backend::Namespace(),
                            ns,
                            "snapshot");

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

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);

    waitForThisBackendWrite(v);
    sleep(100000);

}

INSTANTIATE_TEST(ToolCutTest);

}
// Local Variables: **
// mode: c++ **
// End: **
