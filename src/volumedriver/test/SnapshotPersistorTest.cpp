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
#include "../SnapshotPersistor.h"

#include <algorithm>
#include <iostream>

#include <boost/filesystem/fstream.hpp>

#include <youtils/TestBase.h>

namespace volumedriver
{

namespace fs = boost::filesystem;

class SnapshotPersistorTest
    : public youtilstest::TestBase
{
protected:
    SnapshotPersistorTest()
        : basedir_(getTempPath("SnapshotPersistorTest"))
    {}

    virtual void
    SetUp()
    {
        fs::remove_all(basedir_);
        fs::create_directories(basedir_);
        sp_.reset(new SnapshotPersistor(ParentConfig(parentnamespace,
                                                     "parentsnapshot")));
    }

    virtual void
    TearDown()
    {
        fs::remove_all(basedir_);
        sp_.reset();
    }

    void
    testSnaps(const SnapshotPersistor& sp,
              unsigned numsnaps,
              unsigned numtlogs,
              const std::vector<std::string>& tlognames)
    {
        unsigned numsnapsskipped = 0;
        OrderedTLogNames tlognamesInSnap;

        for(unsigned i = 0; i < numsnaps; i++)
        {
            if(sp.snapshotExists(i))
            {
                ASSERT_NO_THROW(sp_->getTLogsInSnapshot(i,
                                                       tlognamesInSnap));

                ASSERT_EQ(tlognamesInSnap.size(), (1+numsnapsskipped)*numtlogs);
                tlognamesInSnap.clear();

                numsnapsskipped = 0;
            }
            else
            {
                ++numsnapsskipped;
            }
        }
        sp.getCurrentTLogs(tlognamesInSnap);

        ASSERT_EQ(tlognamesInSnap.size(),numsnapsskipped*numtlogs + 1);
        tlognamesInSnap.clear();
        sp.getAllTLogs(tlognamesInSnap,
                       WithCurrent::T);
        ASSERT_TRUE(tlognamesInSnap.size() == tlognames.size());
        for(unsigned i = 0; i< tlognames.size(); i++)
        {
            EXPECT_TRUE(tlognamesInSnap[i] == tlognames[i]);
        }
    }

    void
    testDeserializationConsistencyCheck(bool in_snapshot)
    {
        const size_t num_tlogs = 7;
        std::vector<TLogID> tlog_ids(num_tlogs);
        const size_t snap_idx = 3;

        for (size_t i = 0; i < num_tlogs; ++i)
        {
            tlog_ids[i] = TLog::getTLogIDFromName(sp_->getCurrentTLog());
            if (i == snap_idx)
            {
                sp_->snapshot("snapper");
            }
            else
            {
                sp_->newTLog();
            }
        }

        for (const auto& tid : tlog_ids)
        {
            sp_->setTLogWrittenToBackend(tid);
        }

        if (in_snapshot)
        {
            ASSERT_EQ(1U, sp_->snapshots.size());
            Snapshot& snap = sp_->snapshots.front();

            for (auto& tlog : snap)
            {
                if (tlog.hasID(tlog_ids[snap_idx - 1]))
                {
                    tlog.written_to_backend = false;
                }
            }
        }
        else
        {
            TLogs &tlogs = sp_->current;

            for (auto& tlog : tlogs)
            {
                if (tlog.hasID(tlog_ids[snap_idx + 1]))
                {
                    tlog.written_to_backend = false;
                }
            }
        }

        {
            const fs::path f(FileUtils::create_temp_file(getTempPath("snapshots.xml")));
            ALWAYS_CLEANUP_FILE(f);

            const fs::path g(getTempPath("faulty_xml_archive"));
            ALWAYS_CLEANUP_FILE(g);

            sp_->saveToFile(f.string(), SyncAndRename::T);

            EXPECT_THROW(SnapshotPersistor tmp(f),
                         std::exception);
        }

        {
            for (const auto& tid : tlog_ids)
            {
                sp_->setTLogWrittenToBackend(tid);
            }

            const fs::path
                f(FileUtils::create_temp_file(getTempPath("snapshots.xml")));
            ALWAYS_CLEANUP_FILE(f);

            const fs::path g(getTempPath("faulty_xml_archive"));

            ALWAYS_CLEANUP_FILE(g);
            sp_->saveToFile(f.string(),
                           SyncAndRename::T);
            SnapshotPersistor tmp(f);
        }
    }

    const fs::path basedir_;
    std::unique_ptr<SnapshotPersistor> sp_;
    const Namespace parentnamespace;

};

TEST_F(SnapshotPersistorTest, removeallbuttlast1)
{
    // EXPECT_THROW(sp_->deleteAllButLastSnapshot(),
    //              fungi::IOException);

}

TEST_F(SnapshotPersistorTest, removeallbuttlast2)
{
#define NT                                      \
        tlogs.push_back(sp_->getCurrentTLog()); \
        sp_->newTLog();

    OrderedTLogNames tlogs;
    NT;
    NT;
    tlogs.push_back(sp_->getCurrentTLog());
    sp_->snapshot("First");
    NT;
    NT;
    NT;
    tlogs.push_back(sp_->getCurrentTLog());
    sp_->snapshot("Second");
    NT;
    NT;
    NT;
    NT;
    tlogs.push_back(sp_->getCurrentTLog());
    sp_->snapshot("Third");

    EXPECT_EQ(12U, tlogs.size());

    sp_->deleteAllButLastSnapshot();
    const Snapshots snaps = sp_->getSnapshots();
    EXPECT_EQ(1U, snaps.size());
    EXPECT_TRUE(snaps.front().getName() == "Third");
    EXPECT_EQ(12U, snaps.front().size());

    size_t i = 0;
    for(const auto& tlog : snaps.front())
    {
        EXPECT_TRUE(tlog.getName() == tlogs[i++]);
    }
}

TEST_F(SnapshotPersistorTest, simple)
{
    EXPECT_NO_THROW(TLog::getTLogIDFromName(sp_->getCurrentTLog()));

    EXPECT_THROW(TLog::getTLogIDFromName("tlog_e733574e-5654-4b71-bdde-0489940e68"),
                 fungi::IOException);

    EXPECT_THROW(TLog::getTLogIDFromName("tlog_e733574e-5654-4b71-bdde-0489940e68"),
                 fungi::IOException);

    EXPECT_EQ(parentnamespace,
              sp_->parent()->nspace);

    EXPECT_EQ("parentsnapshot",
              sp_->parent()->snapshot);
}

TEST_F(SnapshotPersistorTest, test1)
{
    const fs::path path(basedir_ / "some_file");
    EXPECT_FALSE(fs::exists(path));
    sp_->saveToFile(path.string(), SyncAndRename::T);
    EXPECT_TRUE(fs::exists(path));
    std::unique_ptr<SnapshotPersistor> p;
    EXPECT_NO_THROW(p.reset(new SnapshotPersistor(path.string())));
    EXPECT_TRUE(p.get());
}

TEST_F(SnapshotPersistorTest, test3)
{
    //sp_->saveToFile();
    // std::string tlogname1 =
    sp_->newTLog();
    //    EXPECT_TRUE(tlogname1 == "tlog_00_0000000000000002");
    // std::string tlogname2 =
    sp_->newTLog();
    //    EXPECT_TRUE(tlogname2 == "tlog_00_0000000000000003");
    // std::string tlogname3 =
    sp_->newTLog();
    //    EXPECT_TRUE(tlogname3 == "tlog_00_0000000000000004");
    sp_->snapshot("FirstSnap");
    //std::string tlogname4 =
    sp_->newTLog();
    //    EXPECT_TRUE(tlogname4 == "tlog_00_0000000000000006");
    // std::string tlogname5 =
    sp_->newTLog();
    //    EXPECT_TRUE(tlogname5 == "tlog_00_0000000000000007");
    // std::string tlogname6 =
    sp_->newTLog();
    //    EXPECT_TRUE(tlogname6 == "tlog_00_0000000000000008");
    ASSERT_THROW(sp_->snapshot("FirstSnap"),fungi::IOException);

    sp_->snapshot("SecondSnap");
    //sp_->saveToFile();
    OrderedTLogNames tlogs;
    sp_->getTLogsInSnapshot(0,
                           tlogs);
//     ASSERT_TRUE(tlogs.size() == 4);
//     EXPECT_TRUE(tlogs[1] == tlogname1);
//     EXPECT_TRUE(tlogs[2] == tlogname2);
//     EXPECT_TRUE(tlogs[3] == tlogname3);

//    ASSERT_NO_THROW(fungi::File::unlink(sp_->getSnapshotsXMLPath()));
}

TEST_F(SnapshotPersistorTest, test4)
{
    const unsigned numsnaps = 50;
    const unsigned numtlogs = 10;

    std::vector<std::string> snapshotnames;
    std::vector<std::string> tlognames;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlognames.push_back(sp_->getCurrentTLog());
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(ss.str());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlognames.push_back(sp_->getCurrentTLog());
        }
        sp_->snapshot(snapshotnames[i]);

    }
//    sp_->saveToFile();

    for(unsigned i = 0; i < numsnaps;i++)
    {
        ASSERT_TRUE(sp_->snapshotExists(snapshotnames[i]));
        ASSERT_TRUE(sp_->getSnapshotName(i) == snapshotnames[i]);
        ASSERT_TRUE(sp_->getSnapshotNum(snapshotnames[i]) == i);
        ASSERT_TRUE(sp_->snapshotExists(i));
        // SnapshotNum prev;
        // if(sp_->getPreviousSnapshot(i,
        //                        prev))
        // {
        //     EXPECT_TRUE(prev == i-1);
        // }
        // else
        // {
        //     EXPECT_TRUE(i == 0);
        // }
        // SnapshotNum next;
        // if(sp_->getNextSnapshot(i,
        //                    next))
        // {
        //     EXPECT_TRUE(next == i+1);
        // }
        // else
        // {
        //     EXPECT_TRUE(i == numsnaps -1);
        // }

        OrderedTLogNames tlognamesInSnap;
        ASSERT_TRUE(sp_->getTLogsInSnapshot(i,
                                           tlognamesInSnap));
        ASSERT_TRUE(tlognamesInSnap.size() == numtlogs);

        for(unsigned j = 0;j < numtlogs; j++)
        {
            ASSERT_TRUE(tlognamesInSnap[j] == tlognames[i*numtlogs +j]);
        }

    }
    //   ASSERT_NO_THROW(fungi::File::unlink(sp_->getSnapshotsXMLPath()));
}

TEST_F(SnapshotPersistorTest, test5)
{
    const unsigned numsnaps = 10;
    const unsigned numtlogs = 50;

    std::vector<std::string> snapshotnames;
    std::vector<std::string> tlognames;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlognames.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlognames.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(ss.str());
        sp_->snapshot(snapshotnames[i]);
    }
    tlognames.push_back(sp_->getCurrentTLog());
//    sp_->saveToFile();

    testSnaps(*sp_, numsnaps, numtlogs, tlognames);
    sp_->deleteSnapshot(3);
    testSnaps(*sp_, numsnaps, numtlogs, tlognames);
    sp_->deleteSnapshot(7);
    testSnaps(*sp_, numsnaps, numtlogs, tlognames);
    sp_->deleteSnapshot(2);
    testSnaps(*sp_, numsnaps, numtlogs, tlognames);
    sp_->deleteSnapshot(0);
    testSnaps(*sp_, numsnaps, numtlogs, tlognames);
    sp_->deleteSnapshot(1);
    testSnaps(*sp_, numsnaps, numtlogs, tlognames);
    sp_->deleteSnapshot(9);
    testSnaps(*sp_, numsnaps, numtlogs, tlognames);
    sp_->deleteSnapshot(4);
    testSnaps(*sp_, numsnaps, numtlogs, tlognames);
    sp_->deleteSnapshot(8);
    testSnaps(*sp_, numsnaps, numtlogs, tlognames);
    sp_->deleteSnapshot(5);
    testSnaps(*sp_, numsnaps, numtlogs, tlognames);
    sp_->deleteSnapshot(6);
    testSnaps(*sp_, numsnaps, numtlogs, tlognames);

    ASSERT_THROW(sp_->deleteSnapshot(8),fungi::IOException);
}

TEST_F(SnapshotPersistorTest, test6)
{
    const unsigned numsnaps = 10;
    const unsigned numtlogs = 50;

    std::vector<std::string> snapshotnames;
    std::vector<std::string> tlognames;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlognames.push_back(sp_->getCurrentTLog());
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(ss.str());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlognames.push_back(sp_->getCurrentTLog());
        }
        sp_->snapshot(snapshotnames[i]);

    }
//    sp_->saveToFile();
    for(unsigned i = 0; i < numsnaps; i++)
    {
        OrderedTLogNames tlognames;
        sp_->getTLogsTillSnapshot(i,
                                tlognames);
        ASSERT_EQ(tlognames.size(), (i + 1) *numtlogs);
        tlognames.clear();

        sp_->getTLogsAfterSnapshot(i,
                                 tlognames);
        ASSERT_EQ(tlognames.size(),
                  (9-i)*numtlogs +1);
    }
}

TEST_F(SnapshotPersistorTest, test7)
{
    const unsigned numsnaps = 10;
    const unsigned numtlogs = 50;

    std::vector<std::string> snapshotnames;
    std::vector<std::string> tlognames;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlognames.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlognames.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(ss.str());
        sp_->snapshot(snapshotnames[i]);

    }
    sp_->deleteTLogsAndSnapshotsAfterSnapshot(5);
    ASSERT_FALSE(sp_->snapshotExists(6));
    ASSERT_TRUE(sp_->snapshotExists(5));
    OrderedTLogNames tlognamesInSnap;

    sp_->getCurrentTLogs(tlognamesInSnap);

    ASSERT_TRUE(tlognamesInSnap.size() == 1);
    tlognamesInSnap.clear();
    sp_->getTLogsInSnapshot(5,
                           tlognamesInSnap);
    ASSERT_TRUE(tlognamesInSnap.size() == numtlogs);
    tlognamesInSnap.clear();

}

TEST_F(SnapshotPersistorTest, test8)
{
    const unsigned numsnaps = 5;
    const unsigned numtlogs = 10;

    std::vector<std::string> snapshotnames;
    std::vector<std::string> tlognames;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlognames.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlognames.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(ss.str());
        sp_->snapshot(snapshotnames[i]);

    }
//    sp_->saveToFile();

    // Move this to snapshotmanagementtest
//    OrderedTLogNames vec;
//     for(size_t i = 0; i < snapshotnames.size(); ++i)
//     {
// //         SnapshotNum num = sp_->getSnapshotNum(snapshotnames[i]);
// //        size_t maxEntries;
// //        sp_->increaseTLogCounter(numtlogs,vec);
// //        sp_->saveToFile();

//         EXPECT_TRUE(vec.size() == numtlogs);

//         for(size_t j = 0;j < tlognames.size(); ++j)
//         {
//             for(size_t k = 0; k < vec.size(); ++k)
//             {
//                 EXPECT_TRUE(tlognames[j] != vec[k]);
//             }
//         }
//         vec.clear();

//     }
}

TEST_F(SnapshotPersistorTest, test9)
{
    const unsigned numsnaps = 5;
    const unsigned numtlogs = 10;

    std::vector<std::string> snapshotnames;
    std::vector<std::string> tlognames;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlognames.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();

            tlognames.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(ss.str());
        sp_->snapshot(snapshotnames[i]);

    }
    //sp_->saveToFile();
    // move to snapshotmanagementtest
//     for(size_t i = 0; i < snapshotnames.size(); ++i)
//     {
//         OrderedTLogNames names;
//         sp_->increaseTLogCounter(1,names);
//         ASSERT_TRUE(names.size() == 1);

//         std::string name= names[0];
//         for(size_t j = 0;j < tlognames.size(); ++j)
//         {
//             EXPECT_TRUE(tlognames[j] != name);
//         }
//     }
}

TEST_F(SnapshotPersistorTest, test10)
{
    const unsigned numsnaps = 5;
    const unsigned numtlogs = 10;

    std::vector<std::string> snapshotnames;
    std::vector<std::string> tlognames;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlognames.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();

            tlognames.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(ss.str());
        sp_->snapshot(snapshotnames[i]);

    }
    // Y42 Move this to snapshotmanagementtest
    //    OrderedTLogNames t;
//     sp_->increaseTLogCounter(1,
//                             t);
//     ASSERT_TRUE(t.size() == 1);
//     EXPECT_TRUE(std::find(tlognames.begin(), tlognames.end(), t[0]) == tlognames.end());
//     OrderedTLogNames tlogsInSnap4;
//     sp_->getTLogsInSnapshot(4,
//                            tlogsInSnap4);
//     ASSERT_TRUE(tlogsInSnap4.size() == numtlogs);

//     sp_->replaceOne(tlogsInSnap4[4],
//                    t[0],
//                    4);
}

TEST_F(SnapshotPersistorTest, test11)
{
    const unsigned numsnaps = 10;
    std::vector<SnapshotNum> nums;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        std::stringstream ss;
        ss << "snapshot_" << i;
        sp_->snapshot(ss.str());
        nums.push_back(sp_->getSnapshotNum(ss.str()));
        //EXPECT_TRUE(not sp_->getSnapshotReadOnly(nums[i]));
        //        sp_->setSnapshotReadOnly(nums[i], i);
        for(unsigned j = 0; j < i; j++)
        {
            //  EXPECT_TRUE(sp_->getSnapshotReadOnly(nums[j]));
        }

    }

    //sp_->unsetSnapshotReadOnly(nums[5],5);

    for(unsigned i = 0; i < numsnaps; ++i)
    {
        //EXPECT_TRUE(sp_->getSnapshotReadOnly(nums[i]));
    }
    //sp_->setSnapshotReadOnly(nums[5],5);

    for(int i = 9; i >= 0; --i)
    {
        //sp_->unsetSnapshotReadOnly(nums[i],i);
        //EXPECT_FALSE(sp_->getSnapshotReadOnly(nums[i]));

        for(unsigned j = 0; j < (unsigned)i; j++)
        {
            //EXPECT_TRUE(sp_->getSnapshotReadOnly(nums[j]));
        }
    }

}

TEST_F(SnapshotPersistorTest, test12)
{
    sp_->snapshot("snap1");
    //SnapshotNum n = sp_->getSnapshotNum("snap1");
    //sp_->unsetSnapshotReadOnly(n,0);
    //EXPECT_FALSE(sp_->getSnapshotReadOnly(n));
    //sp_->setSnapshotReadOnly(n,0);
    //EXPECT_TRUE(sp_->getSnapshotReadOnly(n));
    //sp_->setSnapshotReadOnly(n,0);
    //EXPECT_TRUE(sp_->getSnapshotReadOnly(n));
    //sp_->unsetSnapshotReadOnly(n,0);
    //EXPECT_FALSE(sp_->getSnapshotReadOnly(n));
    //sp_->unsetSnapshotReadOnly(n,0);
    //EXPECT_FALSE(sp_->getSnapshotReadOnly(n));
    //sp_->setSnapshotReadOnly(n, 0);
    //EXPECT_TRUE(sp_->getSnapshotReadOnly(n));
    //sp_->unsetSnapshotReadOnly(n,1);
    //EXPECT_TRUE(sp_->getSnapshotReadOnly(n));


}

TEST_F(SnapshotPersistorTest, test13)
{
    const unsigned numsnaps = 5;
    const unsigned numtlogs = 10;

    std::vector<std::string> snapshotnames;
    std::vector<std::string> tlognames;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlognames.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlognames.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(ss.str());
        sp_->snapshot(snapshotnames[i]);
    }
    SnapshotWork out;
    sp_->getSnapshotScrubbingWork(boost::none, boost::none, out);
    // None of the tlogs have been written to backend!
    EXPECT_TRUE(out.size() == 0);
}

TEST_F(SnapshotPersistorTest, tlogWrittenToBackendConsistencyCheck)
{
    const size_t num_tlogs = 13;
    const size_t snap_idx = 5;
    std::vector<TLogID> tlog_ids(num_tlogs);

    for (size_t i = 0; i < num_tlogs; ++i)
    {
        tlog_ids[i] = TLog::getTLogIDFromName(sp_->getCurrentTLog());

        if (i == snap_idx)
        {
            sp_->snapshot("snapper");
        }
        else
        {
            sp_->newTLog();
        }
    }

    {
        const TLogID& tid = tlog_ids[snap_idx - 1];
        EXPECT_THROW(sp_->setTLogWrittenToBackend(tid),
                     std::exception);
        EXPECT_FALSE(sp_->isTLogWrittenToBackend(tid));
    }

    for (size_t i = 0; i < snap_idx; ++i)
    {
        EXPECT_NO_THROW(sp_->setTLogWrittenToBackend(tlog_ids[i]));
    }

    {
        const TLogID& tid = tlog_ids[snap_idx + 2];
        EXPECT_THROW(sp_->setTLogWrittenToBackend(tid),
                     std::exception);
        EXPECT_FALSE(sp_->isTLogWrittenToBackend(tid));
    }

    for (size_t i = 0; i < num_tlogs; ++i)
    {
        EXPECT_NO_THROW(sp_->setTLogWrittenToBackend(tlog_ids[i]));
    }

    {
        OrderedTLogNames tlog_names;
        sp_->getTLogsWrittenToBackend(tlog_names);
        size_t i = 0;

        for (const auto& id : tlog_ids)
        {
            EXPECT_EQ(tlog_names[i], TLog::getName(id));
            ++i;
        }

        tlog_names.clear();
        sp_->getTLogsNotWrittenToBackend(tlog_names);
        EXPECT_EQ(1U, tlog_names.size());
    }
}

TEST_F(SnapshotPersistorTest, deserialization_consistency_check_1)
{
    testDeserializationConsistencyCheck(true);
}

TEST_F(SnapshotPersistorTest, deserialization_consistency_check_2)
{
    testDeserializationConsistencyCheck(false);
}

TEST_F(SnapshotPersistorTest, no_metadata)
{
    const std::string s("snapshot");
    sp_->snapshot(s);

    const Snapshot snap(sp_->getSnapshot(s));
    EXPECT_EQ(s, snap.getName());
    EXPECT_TRUE(snap.metadata().empty());
}

TEST_F(SnapshotPersistorTest, metadata)
{
    const std::string s("pansshot");
    const SnapshotMetaData m(s.begin(), s.end());
    sp_->snapshot(s, m);

    const Snapshot snap(sp_->getSnapshot(s));
    EXPECT_EQ(s, snap.getName());
    EXPECT_TRUE(m == snap.metadata());
}

TEST_F(SnapshotPersistorTest, excessive_metadata)
{
    const std::string s("tansposh");
    const SnapshotMetaData m(SnapshotPersistor::max_snapshot_metadata_size + 1, 'x');
    EXPECT_THROW(sp_->snapshot(s, m),
                 std::exception);
}

TEST_F(SnapshotPersistorTest, persistent_metadata)
{
    const std::string s("antshops");
    const SnapshotMetaData m(s.begin(), s.end());
    sp_->snapshot(s, m);

    const fs::path p(basedir_ / "some_file");
    ASSERT_FALSE(fs::exists(p));

    sp_->saveToFile(p.string(), SyncAndRename::T);

    sp_.reset(); // make sure we cannot accidentally reuse it
    ASSERT_TRUE(fs::exists(p));

    const SnapshotPersistor sp(p);
    const Snapshot snap(sp.getSnapshot(s));
    EXPECT_EQ(s, snap.getName());
    EXPECT_TRUE(m == snap.metadata());
}

}

// Local Variables: **
// mode: c++ **
// End: **
