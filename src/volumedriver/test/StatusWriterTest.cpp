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
#include "../ScopedVolume.h"
#include "../StatusWriter.h"

namespace volumedrivertest
{
using namespace volumedriver;

class StatusWriterTest
    : public VolManagerTestSetup
{
public:
    StatusWriterTest()
        : VolManagerTestSetup("StatusWriterTest")
    {}
};

TEST_P(StatusWriterTest, write_a_bunch_of_crap)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns = ns1_ptr->ns();

    // Namespace ns;

    ScopedWriteOnlyVolumeWithLocalDeletion v(newWriteOnlyVolume("vol_name",
                                                                ns,
                                                                VolumeConfig::WanBackupVolumeRole::WanBackupBase));


    volumedriver_backup::StatusWriter t(boost::posix_time::seconds(1),
                                        ns);

    const unsigned test_size = 128;
    const unsigned cluster_size = v->getClusterSize();

    t.start(v.get(),
            test_size * cluster_size);

    for(unsigned i = 0; i < test_size; ++i)
    {
        t.add_seen(cluster_size);
        t.add_kept(cluster_size);

        writeToVolume(*v,
                      Lba(0),
                      cluster_size,
                      "immanuel");


        usleep(50000);
    }

    t.finish();

}

INSTANTIATE_TEST(StatusWriterTest);
}

// Local Variables: **
// mode: c++ **
// End: **
