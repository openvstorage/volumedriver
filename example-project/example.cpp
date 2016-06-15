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
#include "volumedriver/Api.h"

using namespace volumedriver;


#define MANAGEMENT_LOCK fungi::ScopedLock __l(api::getManagementMutex())
int
main(int,
     char**)
{

    api::LogInit();

    const boost::filesystem::path configuration_file("./volumedriver.json");

    api::Init(configuration_file,
         0);

    DimensionedValue twelve_gigs("12GiB");

    VolumeId id("example-volume");
    Namespace ns("example-namespace");
    Volume* vol = 0;

    {

        MANAGEMENT_LOCK;

        api::createNewVolume(id,
                             ns,
                             VolumeSize(twelve_gigs.getBytes()),
                             SCOMultiplier(4096),
                             volumedriver::VolumeConfig::default_lba_size_(),
                             volumedriver::VolumeConfig::default_cluster_mult_(),
                             0,
                             std::numeric_limits<uint64_t>::max(),
                             std::unique_ptr<const backend::Guid>(new backend::Guid()));
        vol = api::getVolumePointer(id);
    }

    assert(vol);


    const size_t buf_size(4096);

    std::vector<uint8_t> buf(buf_size, 'a');

    for(uint64_t i = 0; i < 512; ++i)
    {

        api::Write(vol,
                   0,
                   &buf[0],
                   buf_size);
    }


    api::Exit();

}

// Local Variables: **
// compile-command: "make" **
// End: **
