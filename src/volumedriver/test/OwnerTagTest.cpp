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

namespace volumedrivertest
{

using namespace volumedriver;
namespace yt = youtils;

class OwnerTagTest
    : public VolManagerTestSetup
{
public:
    OwnerTagTest()
        : VolManagerTestSetup("OwnerTagTest")
    {}
};

TEST_P(OwnerTagTest, constant_tag)
{
    auto ns(make_random_namespace());
    SharedVolumePtr v = newVolume(*ns);

    const VolumeConfig cfg = v->get_config();
    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    v = localRestart(ns->ns());

    EXPECT_EQ(cfg.owner_tag_,
              v->get_config().owner_tag_);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    restartVolume(cfg);
    v = getVolume(cfg.id_);

    EXPECT_EQ(cfg.owner_tag_,
              v->get_config().owner_tag_);
}

TEST_P(OwnerTagTest, changing_tag)
{
    auto ns(make_random_namespace());
    SharedVolumePtr v = newVolume(*ns);

    const VolumeConfig cfg = v->get_config();
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    const OwnerTag new_tag(new_owner_tag());
    EXPECT_NE(cfg.owner_tag_,
              new_tag);

    {
        fungi::ScopedLock l(VolManager::get()->mgmtMutex_);
        VolManager::get()->backend_restart(cfg.getNS(),
                                           new_tag,
                                           PrefetchVolumeData::F,
                                           IgnoreFOCIfUnreachable::F);
    }

    v = getVolume(cfg.id_);

    EXPECT_EQ(new_tag,
              v->get_config().owner_tag_);
}

INSTANTIATE_TEST(OwnerTagTest);

}
