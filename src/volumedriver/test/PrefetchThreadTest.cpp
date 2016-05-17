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
#include "../PrefetchData.h"
#include "../SCOCache.h"
#include "../VolManager.h"

namespace volumedriver
{

class PrefetchThreadTest
    : public VolManagerTestSetup
{
public:

    PrefetchThreadTest()
        : VolManagerTestSetup("PrefetchThreadTest")
    {}

};

TEST_P(PrefetchThreadTest, test_one)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume("v1",
			  ns);

    const size_t numwrites = 20;
    for(size_t i = 0; i < numwrites; ++i)
    {
        writeToVolume(*v,
                      0,
                      4096,
                      "Superflous");
    }

    syncToBackend(*v);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
}

TEST_P(PrefetchThreadTest, test_two)
{
    // backend::Namespace n1;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& n1 = ns_ptr->ns();

    SharedVolumePtr v = newVolume("v1",
                          n1);
    // Y42 put in VolManagerTestSetup
    const VolumeConfig cfg = v->get_config();
    const uint64_t scoSize = cfg.getSCOSize();


    const size_t numwrites = 20;
    for(size_t i = 0; i < numwrites; ++i)
    {
        writeToVolume(*v,
                      0,
                      scoSize,
                      "Superflous");
    }

    syncToBackend(*v);

    SCONameList list;

    VolManager::get()->getSCOCache()->getSCONameList(n1,
                                        list,
                                        true);

    ASSERT_EQ(numwrites, list.size());

    for(SCONameList::const_iterator it = list.begin();
        it != list.end();
        ++it)
    {
        VolManager::get()->getSCOCache()->removeSCO(n1,
                                       *it,
                                       false);
    }

    {
        SCONameList list;
        VolManager::get()->getSCOCache()->getSCONameList(n1,
                                                         list,
                                                         true);
        ASSERT_TRUE(list.empty());
    }
    //fill the cache with 1 SCO
    SCONameList::const_iterator it = list.begin();
    v->getPrefetchData().addSCO(*it, 1);
    ++it;

    //make sure cachedXValMin_ is set
    VolManager::get()->getSCOCache()->cleanup();

    //make sure next SCO's with low sap are also prefetched as cache is not full
    float sap = 0.01;
    for( ; it != list.end(); ++it)
    {
        v->getPrefetchData().addSCO(*it, sap);
        sap *= 0.9;
    }

    SCONameList list2;
    size_t counter = 0;

    while(list2.size() != numwrites and counter < 60)
    {
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        list2.clear();
        ++counter;
        VolManager::get()->getSCOCache()->getSCONameList(n1,
                                                         list2,
                                                         true);
    }

    ASSERT_EQ(numwrites, list2.size());
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
}

INSTANTIATE_TEST(PrefetchThreadTest);


}

// Local Variables: **
// mode: c++ **
// End: **
