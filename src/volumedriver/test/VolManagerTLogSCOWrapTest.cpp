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
//#include "TransparentTLogBackendReader.h"
//#include "FileTLogBackendReader.h"
#include "../TLogReader.h"
#include "../DataStoreNG.h"

namespace volumedriver
{

class EqualsPredicate
{
public:
    EqualsPredicate(const SCO scoName) : scoName_(scoName) {}
    bool operator()(const SCO &scoName)
    {
        return scoName == scoName_;
    }
private:
    const SCO scoName_;
};

class VolManagerTLogSCOWrapTest
    : public VolManagerTestSetup
{
public:
    VolManagerTLogSCOWrapTest()
        : VolManagerTestSetup("VolManagerTLogSCOWrapTest")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }

    bool checkSCO(Volume &vol, const SCO scoName, bool disposable)
    {
        EqualsPredicate pred(scoName);
        DataStoreNG* dStore = vol.getDataStore();
        SCONameList scoList;

        dStore->listSCOs(scoList, disposable);

        SCONameList::iterator it = std::find_if(scoList.begin(),
                                                scoList.end(),
                                                pred);
        return it != scoList.end();
    }
};


TEST_P(VolManagerTLogSCOWrapTest, DISABLED_one)
{
    const std::string volume = "volume";
    ASSERT_TRUE(0== "Fix this");
    const fs::path tlogDir;// = getTLogDir(volume);
    ASSERT_TRUE(0== "Fix this");
    const fs::path dsDir;// = getDSDir(volume);
    const fs::path dsDirR = dsDir / "R";
    const fs::path dsDirW = dsDir / "W";

    SharedVolumePtr v = newVolume(volume,
			  backend::Namespace());
    //setTLogMaxEntries(v, 3);

    writeToVolume(*v, Lba(0), 4096, "g");
    checkVolume(*v, Lba(0), 4096, "g");

    EXPECT_TRUE(checkSCO(*v, SCO("00_00000001_00"), false));
    EXPECT_FALSE(checkSCO(*v, SCO("00_00000002_00"), false));

    writeToVolume(*v, Lba(0), 4096, "g");

    v->sync();
    ::sync();

    // tlog tlog_00_0000000000000001 should now have 3 entries: 1 sync_to_tc and 2 LOC's

    {
        const OrderedTLogIds currentTLogs(v->getSnapshotManagement().getCurrentTLogs());
        ASSERT_TRUE(currentTLogs.size() > 0);


        const TLogId tlog1 = currentTLogs[0];
        //        const fs::path tlog1d = tlogDir / tlog1;
        //        std::cerr << "inspecting " << tlog1 << std::endl;

        TLogReader r(tlogDir.string(),
                     boost::lexical_cast<std::string>(tlog1),
                     v->getBackendInterface()->clone());
        //        TLogBackendReader *br = brf.create();
        //        TLogReader r(br);
        //        r.rewind();
        const Entry* e;

        ASSERT_TRUE((e = r.nextLocation()));
        //        EXPECT_EQ(Entry::SYNC_TO_TC, e->getType());
        ASSERT_TRUE((e = r.nextLocation()));
        //        EXPECT_EQ(Entry::LOC, e->getType());
        ASSERT_TRUE((e = r.nextLocation()));
        //        EXPECT_EQ(Entry::LOC, e->getType());
        EXPECT_FALSE(r.nextLocation());
    }

    EXPECT_FALSE(checkSCO(*v, SCO("00_00000001_00"), false));
    EXPECT_TRUE(checkSCO(*v, SCO("00_00000001_00"), true));
    EXPECT_TRUE(checkSCO(*v, SCO("00_00000002_00"), false));

    writeToVolume(*v, Lba(0), 4096, "g");
    // expecting new second tlog with a sync2tc and 1 LOC
    // also expecting a second container to be started
    v->sync();
    ::sync();

    {
        const OrderedTLogIds currentTLogs(v->getSnapshotManagement().getCurrentTLogs());

        ASSERT_TRUE(currentTLogs.size() == 2);

        const TLogId tlog2 = currentTLogs[1];
        //const std::string tlog2d = tlogDir + tlog2;
        //        std::cerr << "inspecting " << tlog2 << std::endl;

        TLogReader r(tlogDir.string(),
                     boost::lexical_cast<std::string>(tlog2),
                     v->getBackendInterface()->clone());
        //        TLogBackendReader *br = brf.create();
        //        TLogReader r(br);
        // r.rewind();
        const Entry* e;

        ASSERT_TRUE((e = r.nextLocation()));
        //EXPECT_EQ(Entry::SYNC_TO_TC, e->getType());
        ASSERT_TRUE((e = r.nextLocation()));
        //EXPECT_EQ(Entry::LOC, e->getType());
        EXPECT_FALSE(r.nextLocation());
    }
}

INSTANTIATE_TEST(VolManagerTLogSCOWrapTest);
}

// Local Variables: **
// mode: c++ **
// End: **
