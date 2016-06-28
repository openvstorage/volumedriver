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

    v->setFailOverCacheConfig(foc_ctx->config(DtlMode::Asynchronous));

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
