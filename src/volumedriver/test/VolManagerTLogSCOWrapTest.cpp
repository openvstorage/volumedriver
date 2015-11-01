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

    Volume* v = newVolume(volume,
			  backend::Namespace());
    //setTLogMaxEntries(v, 3);

    writeToVolume(v, 0, 4096, "g");
    checkVolume(v, 0, 4096, "g");

    EXPECT_TRUE(checkSCO(*v, SCO("00_00000001_00"), false));
    EXPECT_FALSE(checkSCO(*v, SCO("00_00000002_00"), false));

    writeToVolume(v, 0, 4096, "g");

    v->sync();
    ::sync();

    // tlog tlog_00_0000000000000001 should now have 3 entries: 1 sync_to_tc and 2 LOC's

    {
        OrderedTLogNames currentTLogs;
        v->getSnapshotManagement().getCurrentTLogs(currentTLogs,AbsolutePath::F);

        ASSERT_TRUE(currentTLogs.size() > 0);


        const std::string tlog1 = currentTLogs[0];
        //        const fs::path tlog1d = tlogDir / tlog1;
        //        std::cerr << "inspecting " << tlog1 << std::endl;

        TLogReader r(tlogDir.string(),
                      tlog1,
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

    writeToVolume(v, 0, 4096, "g");
    // expecting new second tlog with a sync2tc and 1 LOC
    // also expecting a second container to be started
    v->sync();
    ::sync();

    {
        OrderedTLogNames currentTLogs;
        v->getSnapshotManagement().getCurrentTLogs(currentTLogs,AbsolutePath::F);

        ASSERT_TRUE(currentTLogs.size() == 2);

        const std::string tlog2 =  currentTLogs[1];
        //const std::string tlog2d = tlogDir + tlog2;
        //        std::cerr << "inspecting " << tlog2 << std::endl;

        TLogReader r(tlogDir.string(), tlog2, v->getBackendInterface()->clone());
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
