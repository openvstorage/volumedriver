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

#include "VolumeDriverTestConfig.h"

#include <boost/serialization/wrapper.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/scope_exit.hpp>
#include <boost/filesystem/fstream.hpp>

#include <youtils/FileUtils.h>

#include "../Snapshot.h"

namespace volumedriver
{
using namespace youtils;

class SnapshotsTLogsTest
    : public testing::TestWithParam<VolumeDriverTestConfig>
{
    void
    add_tlog(Snapshot& snap, TLog& tlog)
    {
        snap.push_back(tlog);
    }

    void
    add_snap(Snapshots& snaps, const Snapshot& snap)
    {
        snaps.push_back(snap);
    }
};

TEST_F(SnapshotsTLogsTest, test1)
{
    TLogs tlogs;

    Snapshots snaps;
    Snapshot snap1(0, "snap1", tlogs, 0);
    TLog T1;

    add_tlog(snap1, T1);
    add_snap(snaps, snap1);

    Snapshot snap2(1, "snap2", tlogs, 0);
    TLog T2;

    add_tlog(snap2, T2);
    add_snap(snaps, snap2);

    const fs::path tmpfile = yt::FileUtils::temp_path("serialization.txt");
    ALWAYS_CLEANUP_FILE(tmpfile);

    Serialization::serializeAndFlush<boost::archive::xml_oarchive>(tmpfile,
                                                                   BOOST_SERIALIZATION_NVP(snaps));

    Snapshots snaps2;
    fs::ifstream ifs(tmpfile);
    boost::archive::xml_iarchive ia(ifs);
    ia >> BOOST_SERIALIZATION_NVP(snaps2);
    ASSERT_TRUE(snaps == snaps2);
}

}

// Local Variables: **
// mode: c++ **
// End: **
