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

#include <youtils/UUID.h>

#include <backend/BackendInterface.h>

#include <volumedriver/BackendNamesFilter.h>

namespace volumedrivertest
{

using namespace volumedriver;
namespace yt = youtils;

class BackendNamespaceFilterVolumeTest
    : public VolManagerTestSetup
{
protected:
    BackendNamespaceFilterVolumeTest()
        : VolManagerTestSetup("BackendNamespaceFilterVolumeTest")
    {}
};

TEST_P(BackendNamespaceFilterVolumeTest, ditto)
{
    auto wrns(make_random_namespace());

    auto v = newVolume(*wrns);

    auto foc_ctx(start_one_foc());

    v->setFailOverCacheConfig(foc_ctx->config(FailOverCacheMode::Asynchronous));

    writeToVolume(*v,
                  0,
                  v->getClusterSize(),
                  "Not of any importance");

    const std::string snap("snap");
    createSnapshot(*v,
                   snap);

    waitForThisBackendWrite(*v);

    auto bi = v->getBackendInterface()->clone();
    std::list<std::string> objects;
    bi->listObjects(objects);

    BackendNamesFilter f;
    for (const auto& o : objects)
    {
        EXPECT_TRUE(f(o)) << o;
    }
}

INSTANTIATE_TEST(BackendNamespaceFilterVolumeTest);

}

// Local Variables: **
// mode: c++ **
// End: **
