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

#include <fawltyfs/FileSystem.h>

namespace volumedrivertest
{

using namespace volumedriver;
using namespace fawltyfs;
namespace fs = boost::filesystem;

class FawltyTest
    : public VolManagerTestSetup
{
public:
    FawltyTest()
        : VolManagerTestSetup("FawltyTest",
                              UseFawltyMDStores::T,
                              UseFawltyTLogStores::T,
                              UseFawltyDataStores::T)
    {}
};

TEST_P(FawltyTest, faulty_mdstore_test1)
{
    const std::string id1("id1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = VolManagerTestSetup::newVolume(id1,
                                                ns);

    // const std::string id2("id2");


    // Volume* v2 = VolManagerTestSetup::newVolume(id2,
    //                                             id2);


    VolumeRandomIOThread t1(v1,
                            "v1volume");

    // VolumeRandomIOThread t2(v2,
    //                         "v2volume");

    boost::this_thread::sleep_for(boost::chrono::seconds(59));

    t1.stop();

    //    t2.stop();
}

INSTANTIATE_TEST(FawltyTest);

}
