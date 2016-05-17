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
