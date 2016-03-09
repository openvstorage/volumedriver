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
#include "../VolManager.h"

namespace volumedriver
{

class VolManagerThreadTest
    : public VolManagerTestSetup
{
public:
    VolManagerThreadTest()
        : VolManagerTestSetup("VolManagerThreadTest",
                              UseFawltyMDStores::F,
                              UseFawltyTLogStores::F,
                              UseFawltyDataStores::F,
                              0) // num backend threads
    {}
};


TEST_P(VolManagerThreadTest, test1)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns = ns1_ptr->ns();

    // backend::Namespace ns;

    SharedVolumePtr v1 = newVolume("volume1",
			   ns);
    sleep(1);


    for(int i = 0; i < 1024; ++i)
    {
        writeToVolume(*v1,
                      0,
                      4096,
                      "test");
    }
    SCONameList l;

    VolManager::get()->getSCOCache()->getSCONameList(ns,
                                                     l,
                                                     false);

    ASSERT_FALSE(l.empty());

    l.clear();

    VolManager::get()->getSCOCache()->getSCONameList(ns,
                                                     l,
                                                     true);
    ASSERT_TRUE( l.empty());
}

INSTANTIATE_TEST(VolManagerThreadTest);
}

// Local Variables: **
// mode: c++ **
// End: **
