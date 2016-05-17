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
        : VolManagerTestSetup(VolManagerTestSetupParameters("FawltyTest")
                              .use_fawlty_md_stores(UseFawltyMDStores::T)
                              .use_fawlty_tlog_stores(UseFawltyTLogStores::T)
                              .use_fawlty_data_stores(UseFawltyDataStores::T))
    {}
};

TEST_P(FawltyTest, faulty_mdstore_test1)
{
    const std::string id1("id1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v1 = VolManagerTestSetup::newVolume(id1,
                                                ns);

    // const std::string id2("id2");


    // SharedVolumePtr v2 = VolManagerTestSetup::newVolume(id2,
    //                                             id2);


    VolumeRandomIOThread t1(*v1,
                            "v1volume");

    // VolumeRandomIOThread t2(v2,
    //                         "v2volume");

    boost::this_thread::sleep_for(boost::chrono::seconds(59));

    t1.stop();

    //    t2.stop();
}

INSTANTIATE_TEST(FawltyTest);

}
