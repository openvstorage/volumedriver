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
#include "../VolManager.h"

namespace volumedriver
{

class VolManagerThreadTest
    : public VolManagerTestSetup
{
public:
    VolManagerThreadTest()
        : VolManagerTestSetup(VolManagerTestSetupParameters("VolManagerThreadTest")
                              .backend_threads(0))
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
                      Lba(0),
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
