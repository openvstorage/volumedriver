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

#include "ExGTest.h"

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
    : public ExGTest
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

    const fs::path tmpfile = getTempPath("serialization.txt");
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
