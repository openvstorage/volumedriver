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
    auto foc_ctx(start_one_foc());

    const std::string name(yt::UUID().str());
    auto v = newVolume(name, name, VolumeSize(64 << 20));
    v->setFailOverCache(foc_ctx->config());

    writeToVolume(v, "Not of any importance");

    waitForThisBackendWrite(v);

    auto bi = v->getBackendInterface();
    std::list<std::string> objects;
    bi->listObjects(objects);

    BackendNamesFilter f;
    BOOST_FOREACH(const auto& o, objects)
    {
        EXPECT_TRUE(f(o)) << o;
    }
}

INSTANTIATE_TEST(BackendNamespaceFilterVolumeTest);

}

// Local Variables: **
// mode: c++ **
// End: **
