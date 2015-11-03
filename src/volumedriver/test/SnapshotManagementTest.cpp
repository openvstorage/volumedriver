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
                   const SnapshotName& name)
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
    vol_->createSnapshot(SnapshotName("first"));

    ASSERT_THROW(vol_->deleteSnapshot(SnapshotName("__zero__")),
                 fungi::IOException);
    ASSERT_THROW(vol_->deleteSnapshot(SnapshotName("second")),
                 fungi::IOException);
}


TEST_P(SnapshotManagementTest, double_snap_results_in_non_halted_volume)
{
    const SnapshotName first("first");
    vol_->createSnapshot(first);
    waitForThisBackendWrite(vol_);
    ASSERT_THROW(vol_->createSnapshot(SnapshotName(first)),
                 std::exception);
    ASSERT_FALSE(vol_->is_halted());

}

TEST_P(SnapshotManagementTest, getTLogsBetweenSnapshots)
{
    const SnapshotName first("first");
    vol_->createSnapshot(first);
    waitForThisBackendWrite(vol_);

    const SnapshotName second("second");
    vol_->createSnapshot(second);
    waitForThisBackendWrite(vol_);

    const SnapshotName third("third");
    vol_->createSnapshot(third);
    waitForThisBackendWrite(vol_);

    const SnapshotName fourth("fourth");
    vol_->createSnapshot(fourth);
    waitForThisBackendWrite(vol_);
    SnapshotManagement* sman = getSnapshotManagement(vol_);

    SnapshotNum firstn = sman->getSnapshotNumberByName(first);
    SnapshotNum secondn = sman->getSnapshotNumberByName(second);
    SnapshotNum thirdn = sman->getSnapshotNumberByName(third);
    SnapshotNum fourthn = sman->getSnapshotNumberByName(fourth);

    vol_->deleteSnapshot(third);

    OrderedTLogIds out;
    const SnapshotPersistor& pers = getSnapshotManagement(vol_)->getSnapshotPersistor();
    ASSERT_THROW(pers.getTLogsBetweenSnapshots(thirdn,
                                               fourthn,
                                               out,
                                               IncludingEndSnapshot::T),
                 fungi::IOException);
    ASSERT_EQ(0U, out.size());
    // Cannot be tested since it will abort in a non debug release
    ASSERT_THROW(pers.getTLogsBetweenSnapshots(fourthn,
                                               firstn,
                                               out,
                                               IncludingEndSnapshot::T),
                  fungi::IOException);
    ASSERT_EQ(0U, out.size());
    out.clear();

    ASSERT_THROW(pers.getTLogsBetweenSnapshots(thirdn,
                                               10,
                                               out,
                                               IncludingEndSnapshot::T),
                 fungi::IOException);
    ASSERT_EQ(0U, out.size());
    out.clear();

    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(fourthn,
                                                  fourthn,
                                                  out,
                                                  IncludingEndSnapshot::T));
    ASSERT_EQ(2U, out.size());
    out.clear();

    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(fourthn,
                                                  fourthn,
                                                  out,
                                                  IncludingEndSnapshot::F));
    ASSERT_EQ(0U, out.size());
    out.clear();


    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(secondn,
                                                  fourthn,
                                                  out,
                                                  IncludingEndSnapshot::T));

    ASSERT_EQ(2U, out.size());
    out.clear();
    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(secondn,
                                                  fourthn,
                                                  out,
                                                  IncludingEndSnapshot::F));
    EXPECT_EQ(0U, out.size());
    out.clear();

    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(firstn,
                                                  secondn,
                                                  out,
                                                  IncludingEndSnapshot::T));
    EXPECT_EQ(1U, out.size());
    out.clear();

    ASSERT_NO_THROW(pers.getTLogsBetweenSnapshots(firstn,
                                                  secondn,
                                                  out,
                                                  IncludingEndSnapshot::F));
    EXPECT_EQ(0U, out.size());
}

TEST_P(SnapshotManagementTest, TheRevengeOfGetScrubbingWorkTest)
{
    youtils::SourceOfUncertainty soc;
    const unsigned size = soc(5, 10);

    std::vector<SnapshotName> snapshot_names;
    for( unsigned i = 0; i < size; ++i)
    {
        snapshot_names.push_back(boost::lexical_cast<SnapshotName>(i));
    }

    for(unsigned i = 0; i < size; ++i)
    {
        vol_->createSnapshot(snapshot_names[i]);
        waitForThisBackendWrite(vol_);
    }

    SnapshotManagement* sman = getSnapshotManagement(vol_);

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


    EXPECT_THROW(sman->getSnapshotScrubbingWork(SnapshotName("nothing"),
                                                SnapshotName("more_nothing"),
                                                out),
                 fungi::IOException);
    EXPECT_EQ(0U, out.size());
    out.clear();

    for(unsigned i = 0; i < size; ++i)
    {
        sman->getSnapshotScrubbingWork(snapshot_names[i],
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
        waitForThisBackendWrite(vol_);
        vol_->createSnapshot(boost::lexical_cast<SnapshotName>(i));
    }

    SnapshotManagement* c = getSnapshotManagement(vol_);

    ASSERT_EQ(10U *4096U,
              c->getSnapshotPersistor().getBackendSize(SnapshotName("9"),
                                                       boost::none));

    for(size_t j = 0; j < 9; j++)
    {
        for (size_t i = 0; i < j; ++i)
        {
            ASSERT_EQ((j-i) * 4096U,
                      c->getSnapshotPersistor().getBackendSize(boost::lexical_cast<SnapshotName>(j),
                                                               boost::lexical_cast<SnapshotName>(i)));
        }
    }
}

TEST_P(SnapshotManagementTest, deleteSnaps2)
{
    const SnapshotName snap("first");
    vol_->createSnapshot(snap);
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

    vol_->deleteSnapshot(snap);
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
    const SnapshotName snapname("snap1");
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
    const SnapshotName snapname("snap1");
    createSnapshot(c,snapname);
    waitForThisBackendWrite(vol_);
    std::list<std::string> tlognames;

    backendRegex(Namespace(ns()),
                 "tlog_.*",
                 tlognames);
    ASSERT_EQ(1U, tlognames.size());

    ASSERT_TRUE(tlognames.size() > 0);

    for (const std::string& tlogname : tlognames)
    {
        EXPECT_TRUE(c->getSnapshotPersistor().isTLogWrittenToBackend(boost::lexical_cast<TLogId>(tlogname)));
    }

    OrderedTLogIds writtentobackend;
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

    const SnapshotName snapname("snap1");
    createSnapshot(c,snapname);

    SnapshotWork out;
    waitForThisBackendWrite(vol_);

    c->getSnapshotScrubbingWork(boost::none, boost::none, out);
    ASSERT_TRUE(out.size() == 1);

    OrderedTLogIds tlogs;
    c->getTLogsInSnapshot(c->getSnapshotNumberByName(snapname),
                          tlogs);

    SnapshotWorkUnit& one = out[0];
    EXPECT_EQ(snapname,
              one);

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

    const SnapshotName snapname("snap1");
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
    const SnapshotName snapname("snap1");
    createSnapshot(c,snapname);
    //SnapshotNum num1 = c.getSnapshotNumberByName(snapname);
    for(size_t i = 0; i < 10; ++i)
    {
        writeToVolume(vol_,0,4096,"eearvwdtrd");
    }
    const SnapshotName snapname2("snap2");
    createSnapshot(c,snapname2);
    // SnapshotNum num2 = c.getSnapshotNumberByName(snapname2);

    for(size_t i = 0; i < 10; ++i)
    {
        writeToVolume(vol_,0,4096,"help");
    }
    const SnapshotName snapname3("snap3");
    createSnapshot(c,snapname3);
    // SnapshotNum num3 = c.getSnapshotNumberByName(snapname3);

//     c.setSnapshotReadOnly(num2, 0);

//     SnapshotWork out;
//     waitForThisBackendWrite();

//     c.getSnapshotScrubbingWork(out);
//     ASSERT_TRUE(out.size() == 1);

//     OrderedTLogIds tlogs;
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
    const SnapshotName snapname("snap1");
    createSnapshot(c,snapname);
    //SnapshotNum num1 = c.getSnapshotNumberByName(snapname);
    for(size_t i = 0; i < 10; ++i)
    {
        write(0,4096);
    }
    const SnapshotName snapname2("snap2");
    createSnapshot(c,snapname2);
    //SnapshotNum num2 = c.getSnapshotNumberByName(snapname2);

    for(size_t i = 0; i < 10; ++i)
    {
        write(0,4096);
    }
    const SnapshotName snapname3("snap3");
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
//     OrderedTLogIds tlogs2;
//     c.getTLogsInSnapshot(num2,
//                          tlogs2,
//                          false);
//     EXPECT_TRUE(first.second == tlogs2);

//     SnapshotWorkUnit& second = out[1];

//     ASSERT_TRUE(second.first == snapname3);
//     OrderedTLogIds tlogs3;
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

    const SnapshotName snapname1("snap1");
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
        createSnapshot(c, SnapshotName(ss.str()));
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

    c.deleteSnapshot(SnapshotName("snap_3"));
    c.deleteSnapshot(SnapshotName("snap_7"));
    c.deleteSnapshot(SnapshotName("snap_11"));

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
    const SnapshotName snap1("snap1");

    writeToVolume(vol_, 0, vol_->getClusterSize(), snap1);
    VolManagerTestSetup::createSnapshot(vol_, snap1);

    waitForThisBackendWrite(vol_);

    const SnapshotName snap2("snap2");
    writeToVolume(vol_, 0, vol_->getClusterSize(), snap2);
    vol_->sync();

    const OrderedTLogIds tlogpaths(getSnapshotManagement(vol_)->getCurrentTLogs());
    EXPECT_LT(0U,
              tlogpaths.size());

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
