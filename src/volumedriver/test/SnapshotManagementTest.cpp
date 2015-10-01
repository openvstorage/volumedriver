// Copyright 2015 Open vStorage NV
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
#include "VolManagerTestSetup.h"

#include <fstream>
#include <algorithm>
#include <boost/foreach.hpp>

#include <youtils/SourceOfUncertainty.h>

#include <backend/BackendInterface.h>

#include "../SnapshotManagement.h"
#include "../VolManager.h"
#include "../Volume.h"

namespace volumedrivertest
{
using namespace volumedriver;

class SnapshotManagementTest
    : public VolManagerTestSetup
{
  protected:
    SnapshotManagementTest()
    : VolManagerTestSetup("SnapshotManagementTestVolume")
    , vol_(0)
    , volname_("snapshot-test-vol")
    {}

    void
    createSnapshot(SnapshotManagement* c,
                   const std::string& name)
    {
        c->createSnapshot(name,
                          0);
    }
    const Namespace&
    ns()
    {
        EXPECT_TRUE(static_cast<bool>(ns_ptr_));
        return ns_ptr_->ns();
    }

    void
    SetUp()
    {
        VolManagerTestSetup::SetUp();
        ns_ptr_ = make_random_namespace();
        vol_ = newVolume(VolumeId(volname_),
                         ns_ptr_->ns());
    }

    void
    TearDown()
    {
        destroyVolume(vol_,
                      DeleteLocalData::T,
                      RemoveVolumeCompletely::T);
        ns_ptr_.reset();

        VolManagerTestSetup::TearDown();
    }

    Volume* vol_;
    const std::string volname_;
    std::unique_ptr<WithRandomNamespace> ns_ptr_;

};


TEST_P(SnapshotManagementTest, deleteSnaps1)
{
    vol_->createSnapshot("first");

    ASSERT_THROW(vol_->deleteSnapshot("__zero__"),fungi::IOException);
    ASSERT_THROW(vol_->deleteSnapshot("second"),fungi::IOException);
}


TEST_P(SnapshotManagementTest, double_snap_results_in_non_halted_volume)
{
    vol_->createSnapshot("first");
    waitForThisBackendWrite(vol_);
    ASSERT_THROW(vol_->createSnapshot("first"), std::exception);
    ASSERT_FALSE(vol_->is_halted());

}

TEST_P(SnapshotManagementTest, getTLogsBetweenSnapshots)
{
    vol_->createSnapshot("first");
    waitForThisBackendWrite(vol_);
    vol_->createSnapshot("second");
    waitForThisBackendWrite(vol_);
    vol_->createSnapshot("third");
    waitForThisBackendWrite(vol_);
    vol_->createSnapshot("fourth");
    waitForThisBackendWrite(vol_);
    SnapshotManagement* sman = getSnapshotManagement(vol_);

    SnapshotNum first = sman->getSnapshotNumberByName("first");
    SnapshotNum second = sman->getSnapshotNumberByName("second");
    SnapshotNum third = sman->getSnapshotNumberByName("third");
    SnapshotNum fourth = sman->getSnapshotNumberByName("fourth");

    vol_->deleteSnapshot("third");

    OrderedTLogNames out;
    const SnapshotPersistor& pers = getSnapshotManagement(vol_)->getSnapshotPersistor();
    ASSERT_THROW(pers.getTLogsBetweenSnapshots(third,
                                               fourth,
                                               out,
                                               IncludingEndSnapshot::T),
                 fungi::IOException);
    ASSERT_EQ(0U, out.size());
    // Cannot be tested since it will abort in a non debug release
    ASSERT_THROW(pers.getTLogsBetweenSnapshots(fourth,
                                                first,
                                                out,
                                               IncludingEndSnapshot::T),
                  fungi::IOException);
    ASSERT_EQ(0U, out.size());
    out.clear();

    ASSERT_THROW(pers.getTLogsBetweenSnapshots(third,
                                               10,
                                               out,
                                               IncludingEndSnapshot::T),
                 fungi::IOException);
    ASSERT_EQ(0U, out.size());
    out.clear();

    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(fourth,
                                               fourth,
                                               out,
                                               IncludingEndSnapshot::T));
    ASSERT_EQ(2U, out.size());
    out.clear();

    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(fourth,
                                               fourth,
                                               out,
                                               IncludingEndSnapshot::F));
    ASSERT_EQ(0U, out.size());
    out.clear();


    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(second,
                                                  fourth,
                                                  out,
                                                  IncludingEndSnapshot::T));

    ASSERT_EQ(2U, out.size());
    out.clear();
    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(second,
                                                  fourth,
                                                  out,
                                                  IncludingEndSnapshot::F));
    EXPECT_EQ(0U, out.size());
    out.clear();

    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(first,
                                                  second,
                                                  out,
                                                  IncludingEndSnapshot::T));
    EXPECT_EQ(1U, out.size());
    out.clear();

    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(first,
                                                  second,
                                                  out,
                                                  IncludingEndSnapshot::F));
    EXPECT_EQ(0U, out.size());
}

TEST_P(SnapshotManagementTest, TheRevengeOfGetScrubbingWorkTest)
{
    youtils::SourceOfUncertainty soc;
    const unsigned size = soc(5, 10);

    std::vector<std::string> snapshot_names;
    for( unsigned i = 0; i < size; ++i)
    {
        std::stringstream ss;
        ss << i;
        snapshot_names.push_back(ss.str());
    }

    for(unsigned i = 0; i < size; ++i)
    {
        vol_->createSnapshot(snapshot_names[i]);
        waitForThisBackendWrite(vol_);
    }

    SnapshotManagement* sman = getSnapshotManagement(vol_);

    typedef boost::optional<std::string> arg_type;

    SnapshotWork out;


    EXPECT_NO_THROW(sman->getSnapshotScrubbingWork(boost::none,
                                                   boost::none,
                                                   out));
    EXPECT_EQ(out.size(), size);
    for(unsigned i = 0; i < size; ++i)
    {
        EXPECT_EQ(out[i], snapshot_names[i]) << "Size was " << size << "i was" << i;
    }

    out.clear();


    EXPECT_THROW(sman->getSnapshotScrubbingWork(arg_type("nothing"),
                                                arg_type("more_nothing"),
                                                out),
                 fungi::IOException);
    EXPECT_EQ(0U, out.size());
    out.clear();

    for(unsigned i = 0; i < size; ++i)
    {
        sman->getSnapshotScrubbingWork(arg_type(snapshot_names[i]),
                                       boost::none,
                                       out);
        EXPECT_EQ(out.size(), size - (i + 1)) << "Size tested was " << size << "i was " << i;
        for(unsigned k = 0; k < size - (i+1); ++k)
        {
            EXPECT_EQ(out[k], snapshot_names[i+k+1]);
        }
        out.clear();
    }

}


TEST_P(SnapshotManagementTest, backendSizeBetweenSnapshots)
{

    for(size_t i = 0; i < 10; ++i)
    {

        writeToVolume(vol_, 0,  4096,"blue");
        std::stringstream ss;
        ss << i;
        waitForThisBackendWrite(vol_);

        vol_->createSnapshot(ss.str());
    }

    SnapshotManagement* c = getSnapshotManagement(vol_);

    ASSERT_EQ(10U *4096U,
              c->getSnapshotPersistor().getBackendSize("9",
                                                       boost::none));

    for(size_t j = 0; j < 9; j++)
    {
        std::stringstream js;
        js << j;

        for (size_t i = 0; i < j; ++i)
        {
            std::stringstream ss;
            ss << i;
            ASSERT_EQ((j-i) * 4096U,
                      c->getSnapshotPersistor().getBackendSize(js.str(),
                                                               ss.str()));
        }
    }
}

TEST_P(SnapshotManagementTest, deleteSnaps2)
{
    vol_->createSnapshot("first");
    waitForThisBackendWrite(vol_);

    std::list<std::string> names;
    backendRegex(Namespace(ns()),
                "tlog_.*",
                names);

    EXPECT_EQ(1U, names.size());
    names.clear();

    backendRegex(Namespace(ns()),
                "snapshots\\.xml",
                names);

    // EXPECT_EQ(names.size(),(size_t)5);
    EXPECT_EQ(1U, names.size());
    names.clear();


    vol_->deleteSnapshot("first");
    waitForThisBackendWrite(vol_);
    // scrubbing disabled waitForScrubJobs();

    backendRegex(Namespace(ns()),
                "tlog_.*",
                names);
    EXPECT_EQ(1U, names.size());
    names.clear();
    backendRegex(Namespace(ns()),
                "snapshots\\.xml",
                names);
    //EXPECT_EQ(names.size(),(size_t)6);
    EXPECT_EQ(1U, names.size());
}

TEST_P(SnapshotManagementTest, test1)
{
    SnapshotManagement* c = getSnapshotManagement(vol_);
    std::string snapname("snap1");
    createSnapshot(c,snapname);
    EXPECT_TRUE(c->snapshotExists(snapname));
    SnapshotNum n = c->getSnapshotNumberByName(snapname);

    EXPECT_EQ(n, (SnapshotNum)0);
    waitForThisBackendWrite(vol_);

    std::list<std::string> tlognames;
    backendRegex(Namespace(ns()),
                 "tlog_.*",
                 tlognames);
    ASSERT_EQ(1U, tlognames.size());
}

TEST_P(SnapshotManagementTest, test2)
{
    SnapshotManagement* c = getSnapshotManagement(vol_);
    //c->setMaxTLogEntries(4);
    for(size_t i = 0; i < 10; ++i)
    {
        writeToVolume(vol_,0,4096,"blue");
    }
    std::string snapname("snap1");
    createSnapshot(c,snapname);
    waitForThisBackendWrite(vol_);
    std::list<std::string> tlognames;

    backendRegex(Namespace(ns()),
                 "tlog_.*",
                 tlognames);
    ASSERT_EQ(1U, tlognames.size());

    ASSERT_TRUE(tlognames.size() > 0);
    BOOST_FOREACH(const std::string& tlogname, tlognames)
    {
        EXPECT_TRUE(c->getSnapshotPersistor().isTLogWrittenToBackend(tlogname));
    }

    OrderedTLogNames writtentobackend;
    // c.getSnapshotPersistor().getTLogsWrittenToBackend(writtentobackend);
    // for(size_t i = 0; i < writtentobackend.size();++i)
    // {
    //     EXPECT_TRUE(std::count(tlognames.begin(), tlognames.end(),writtentobackend[i]) == 1);
    // }
    std::list<std::string> snapshotnames;
    backendRegex(Namespace(ns()), "snapshots\\.xml", snapshotnames);
    // scrubbing disabled EXPECT_EQ(snapshotnames.size(),(size_t)2);
    EXPECT_EQ(1U, snapshotnames.size());
}

TEST_P(SnapshotManagementTest, test3)
{
    SnapshotManagement* c = getSnapshotManagement(vol_);
    //c->setMaxTLogEntries(4);
    for(size_t i = 0; i < 10; ++i)
    {
        writeToVolume(vol_, 0, 4096, "wartdebever");
    }

    std::string snapname("snap1");
    createSnapshot(c,snapname);

    SnapshotWork out;
    waitForThisBackendWrite(vol_);

    c->getSnapshotScrubbingWork(boost::none, boost::none, out);
    ASSERT_TRUE(out.size() == 1);

    OrderedTLogNames tlogs;
    c->getTLogsInSnapshot(c->getSnapshotNumberByName("snap1"),
                       tlogs,
                       AbsolutePath::F);
    SnapshotWorkUnit& one = out[0];
    EXPECT_TRUE(one == "snap1");

    //    EXPECT_TRUE(one.second == tlogs);

}

TEST_P(SnapshotManagementTest, test4)
{
    SnapshotManagement* c = getSnapshotManagement(vol_);
    //c->setMaxTLogEntries(4);
    for(size_t i = 0; i < 10; ++i)
    {
        writeToVolume(vol_,0,4096,"dartbewever");
    }
    std::string snapname("snap1");
    createSnapshot(c,snapname);
    // SnapshotNum num = c.getSnapshotNumberByName("snap1");
    //    c.setSnapshotReadOnly(num, 0);

    SnapshotWork out;
    waitForThisBackendWrite(vol_);

//     c.getSnapshotScrubbingWork(out);
//     ASSERT_TRUE(out.size() == 0);

}

TEST_P(SnapshotManagementTest, test5)
{

    SnapshotManagement* c = getSnapshotManagement(vol_);
    //gc->setMaxTLogEntries(4);
    for(size_t i = 0; i < 10; ++i)
    {
        writeToVolume(vol_,0,4096,"drtdwvraee");

    }
    std::string snapname("snap1");
    createSnapshot(c,snapname);
    //SnapshotNum num1 = c.getSnapshotNumberByName(snapname);
    for(size_t i = 0; i < 10; ++i)
    {
        writeToVolume(vol_,0,4096,"eearvwdtrd");
    }
    std::string snapname2("snap2");
    createSnapshot(c,snapname2);
    // SnapshotNum num2 = c.getSnapshotNumberByName(snapname2);

    for(size_t i = 0; i < 10; ++i)
    {
        writeToVolume(vol_,0,4096,"help");
    }
    std::string snapname3("snap3");
    createSnapshot(c,snapname3);
    // SnapshotNum num3 = c.getSnapshotNumberByName(snapname3);

//     c.setSnapshotReadOnly(num2, 0);

//     SnapshotWork out;
//     waitForThisBackendWrite();

//     c.getSnapshotScrubbingWork(out);
//     ASSERT_TRUE(out.size() == 1);

//     OrderedTLogNames tlogs;
//     c.getTLogsInSnapshot(num3,
//                        tlogs,
//                        false);
//     SnapshotWorkUnit& one = out[0];
//     EXPECT_TRUE(one.first == snapname3);

//     EXPECT_TRUE(one.second == tlogs);

}
#if 0
TEST_P(SnapshotManagementTest, test6)
{
    blockBackendWrites();

    SnapshotManagement& c = getSnapshotManagement();
    c.setMaxTLogEntries(4);
    for(size_t i = 0; i < 10; ++i)
    {
        write(0,4096);
    }
    std::string snapname("snap1");
    createSnapshot(c,snapname);
    //SnapshotNum num1 = c.getSnapshotNumberByName(snapname);
    for(size_t i = 0; i < 10; ++i)
    {
        write(0,4096);
    }
    std::string snapname2("snap2");
    createSnapshot(c,snapname2);
    //SnapshotNum num2 = c.getSnapshotNumberByName(snapname2);

    for(size_t i = 0; i < 10; ++i)
    {
        write(0,4096);
    }
    std::string snapname3("snap3");
    createSnapshot(c,snapname3);
    //SnapshotNum num3 = c.getSnapshotNumberByName(snapname3);


    SnapshotWork out;
//    waitForThisBackendWrite();

    c.getSnapshotScrubbingWork(out);
    EXPECT_EQ(0U, out.size());
    unblockBackendWrites();
    waitForThisBackendWrite();

//     c.setSnapshotReadOnly(num1, 0);

//     c.getSnapshotScrubbingWork(out);
//     ASSERT_TRUE(out.size() == 2);
//     SnapshotWorkUnit& first = out[0];

//     ASSERT_TRUE(first.first == snapname2);
//     OrderedTLogNames tlogs2;
//     c.getTLogsInSnapshot(num2,
//                          tlogs2,
//                          false);
//     EXPECT_TRUE(first.second == tlogs2);

//     SnapshotWorkUnit& second = out[1];

//     ASSERT_TRUE(second.first == snapname3);
//     OrderedTLogNames tlogs3;
//     c.getTLogsInSnapshot(num3,
//                          tlogs3,
//                          false);
//     EXPECT_TRUE(second.second == tlogs3);

}


TEST_P(SnapshotManagementTest, test7)
{
    SnapshotManagement* c = getSnapshotManagement();
    for(int i = 0; i < 128; ++i)
    {
        writeTovolume(vol_,0, 4096,"val");
    }


    std::string snapname1("snap1");
    read(20,4096, std::string(1,'\0'));
    createSnapshot(c,snapname1);

    for(int i = 0; i < 128; ++i)
    {
        writeToVolume(vol_, 20, 4096,"val");
    }

    {
        fungi::ScopedLock l(getVolManagerLock());
        vol_->restoreSnapshot("snap1");
    }

    read(20,4096,std::string(1,'\0'));
}

TEST_P(SnapshotManagementTest, test8)
{
    SnapshotManagement& c = getSnapshotManagement();
    for(int i = 0; i < 25; ++i)
    {
        write(0,4096,"val");
        std::stringstream ss;
        ss << "snap_" << i;
        blockBackendWrites();
        createSnapshot(c,ss.str());
        //EXPECT_FALSE(c.isSnapshotWrittenToBackend(ss.str()));
        unblockBackendWrites();
    }
    waitForThisBackendWrite();
    for(int i = 0; i < 25; ++i)
    {
        std::stringstream ss;
        ss << "snap_" << i;
        //        EXPECT_TRUE(c.isSnapshotWrittenToBackend(ss.str()));
    }

    c.deleteSnapshot("snap_3");
    c.deleteSnapshot("snap_7");
    c.deleteSnapshot("snap_11");


    for(int i = 0; i < 25; ++i)
    {
        std::stringstream ss;
        ss << "snap_" << i;
        if(i != 3 and
           i != 7 and
           i != 11)
        {
            //EXPECT_TRUE(c.isSnapshotWrittenToBackend(ss.str()));
        }
        else
        {
            //EXPECT_THROW(c.isSnapshotWrittenToBackend(ss.str()),
            // fungi::IOException);
        }

    }
}
TEST_P(SnapshotManagementTest, test9)
{
    SnapshotManagement& c = getSnapshotManagement();
    const std::string tlogName = c.getSnapshotPersistor().getCurrentTLog();
    vol_->scheduleBackendSync();
    waitForThisBackendWrite();
    const std::string tlogNameNow = c.getSnapshotPersistor().getCurrentTLog();
    ASSERT_TRUE(tlogName != tlogNameNow);
    ASSERT_TRUE(c.getSnapshotPersistor().isTLogWrittenToBackend(tlogName));

}
#endif

// better move to SimpleVolumeTest?
TEST_P(SnapshotManagementTest, dontLeakTLogCheckSumsOnRestore)
{
    const std::string snap1("snap1");

    writeToVolume(vol_, 0, vol_->getClusterSize(), snap1);
    VolManagerTestSetup::createSnapshot(vol_, snap1);

    waitForThisBackendWrite(vol_);

    const std::string snap2("snap2");
    writeToVolume(vol_, 0, vol_->getClusterSize(), snap2);
    vol_->sync();

    OrderedTLogNames tlogpaths;
    getSnapshotManagement(vol_)->getCurrentTLogs(tlogpaths, AbsolutePath::T);
    EXPECT_LT(0U, tlogpaths.size());

    //CheckSumStore<std::string>& chksums = getCheckSumStore();

    // BOOST_FOREACH(const TLogName& n, tlogpaths)
    // {
    //     const fs::path p(n);
    //     EXPECT_TRUE(fs::exists(p));
    //     const std::string tlogname = p.leaf();
    //     EXPECT_NO_THROW(chksums.find(tlogname));
    // }


    restoreSnapshot(vol_, snap1);

    // BOOST_FOREACH(const TLogName& n, tlogpaths)
    // {
    //     const fs::path p(n);
    //     EXPECT_FALSE(fs::exists(p));
    //     const std::string tlogname = p.leaf();
    //     EXPECT_THROW(chksums.find(tlogname),
    //                  CheckSumStoreException);
    // }

    checkVolume(vol_, 0, vol_->getClusterSize(), snap1);

}

INSTANTIATE_TEST(SnapshotManagementTest);
}

// Local Variables: **
// mode: c++ **
// End: **
