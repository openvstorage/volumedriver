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
#include "../SnapshotPersistor.h"

#include <algorithm>
#include <iostream>

#include <boost/filesystem/fstream.hpp>

#include <gtest/gtest.h>

#include <youtils/FileUtils.h>

namespace volumedriver
{

namespace fs = boost::filesystem;
namespace yt = youtils;

class SnapshotPersistorTest
    : public testing::Test
{
protected:
    SnapshotPersistorTest()
        : basedir_(yt::FileUtils::temp_path("SnapshotPersistorTest"))
    {}

    virtual void
    SetUp()
    {
        fs::remove_all(basedir_);
        fs::create_directories(basedir_);
        sp_.reset(new SnapshotPersistor(ParentConfig(parentnamespace,
                                                     SnapshotName("parentsnapshot"))));
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
              const std::vector<TLogId>& tlog_ids)
    {
        unsigned numsnapsskipped = 0;
        OrderedTLogIds tlog_ids_in_snap;

        for(unsigned i = 0; i < numsnaps; i++)
        {
            if(sp.snapshotExists(i))
            {
                ASSERT_NO_THROW(sp_->getTLogsInSnapshot(i,
                                                       tlog_ids_in_snap));

                ASSERT_EQ(tlog_ids_in_snap.size(), (1+numsnapsskipped)*numtlogs);
                tlog_ids_in_snap.clear();

                numsnapsskipped = 0;
            }
            else
            {
                ++numsnapsskipped;
            }
        }
        sp.getCurrentTLogs(tlog_ids_in_snap);

        ASSERT_EQ(tlog_ids_in_snap.size(),
                  numsnapsskipped*numtlogs + 1);

        tlog_ids_in_snap.clear();
        sp.getAllTLogs(tlog_ids_in_snap,
                       WithCurrent::T);

        ASSERT_EQ(tlog_ids_in_snap.size(),
                  tlog_ids.size());

        for(unsigned i = 0; i< tlog_ids.size(); i++)
        {
            EXPECT_EQ(tlog_ids_in_snap[i],
                      tlog_ids[i]);
        }
    }

    void
    testDeserializationConsistencyCheck(bool in_snapshot)
    {
        const size_t num_tlogs = 7;
        std::vector<TLogId> tlog_ids(num_tlogs);
        const size_t snap_idx = 3;

        for (size_t i = 0; i < num_tlogs; ++i)
        {
            tlog_ids[i] = boost::lexical_cast<TLogId>(sp_->getCurrentTLog());
            if (i == snap_idx)
            {
                sp_->snapshot(SnapshotName("snapper"));
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
                if (tlog.id() == tlog_ids[snap_idx - 1])
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
                if (tlog.id() == tlog_ids[snap_idx + 1])
                {
                    tlog.written_to_backend = false;
                }
            }
        }

        {
            const fs::path f(FileUtils::create_temp_file(yt::FileUtils::temp_path("snapshots.xml")));
            ALWAYS_CLEANUP_FILE(f);

            const fs::path g(yt::FileUtils::temp_path("faulty_xml_archive"));
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
                f(FileUtils::create_temp_file(yt::FileUtils::temp_path("snapshots.xml")));
            ALWAYS_CLEANUP_FILE(f);

            const fs::path g(yt::FileUtils::temp_path("faulty_xml_archive"));

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

    OrderedTLogIds tlogs;
    NT;
    NT;
    tlogs.push_back(sp_->getCurrentTLog());
    sp_->snapshot(SnapshotName("First"));
    NT;
    NT;
    NT;
    tlogs.push_back(sp_->getCurrentTLog());
    sp_->snapshot(SnapshotName("Second"));
    NT;
    NT;
    NT;
    NT;
    tlogs.push_back(sp_->getCurrentTLog());
    sp_->snapshot(SnapshotName("Third"));

    EXPECT_EQ(12U, tlogs.size());

    sp_->deleteAllButLastSnapshot();
    const Snapshots snaps = sp_->getSnapshots();
    EXPECT_EQ(1U, snaps.size());
    EXPECT_EQ(SnapshotName("Third"),
              snaps.front().getName());
    EXPECT_EQ(12U, snaps.front().size());

    size_t i = 0;
    for(const auto& tlog : snaps.front())
    {
        EXPECT_EQ(tlog.id(),
                  tlogs[i++]);
    }
}

TEST_F(SnapshotPersistorTest, simple)
{
    EXPECT_NO_THROW(boost::lexical_cast<std::string>(sp_->getCurrentTLog()));

    EXPECT_THROW(boost::lexical_cast<TLogId>("tlog_e733574e-5654-4b71-bdde-0489940e68"),
                 boost::bad_lexical_cast);

    EXPECT_THROW(boost::lexical_cast<TLogId>("tlog_e733574e-5654-4b71-bdde-0489940e68"),
                 boost::bad_lexical_cast);

    EXPECT_EQ(parentnamespace,
              sp_->parent()->nspace);

    EXPECT_EQ(SnapshotName("parentsnapshot"),
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
    sp_->snapshot(SnapshotName("FirstSnap"));
    //std::string tlogname4 =
    sp_->newTLog();
    //    EXPECT_TRUE(tlogname4 == "tlog_00_0000000000000006");
    // std::string tlogname5 =
    sp_->newTLog();
    //    EXPECT_TRUE(tlogname5 == "tlog_00_0000000000000007");
    // std::string tlogname6 =
    sp_->newTLog();
    //    EXPECT_TRUE(tlogname6 == "tlog_00_0000000000000008");
    ASSERT_THROW(sp_->snapshot(SnapshotName("FirstSnap")),
                 fungi::IOException);

    sp_->snapshot(SnapshotName("SecondSnap"));
    //sp_->saveToFile();
    OrderedTLogIds tlogs;
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

    std::vector<SnapshotName> snapshotnames;
    std::vector<TLogId> tlog_ids;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlog_ids.push_back(sp_->getCurrentTLog());
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(SnapshotName(ss.str()));
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlog_ids.push_back(sp_->getCurrentTLog());
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

        OrderedTLogIds tlogIdsInSnap;
        ASSERT_TRUE(sp_->getTLogsInSnapshot(i,
                                           tlogIdsInSnap));
        ASSERT_TRUE(tlogIdsInSnap.size() == numtlogs);

        for(unsigned j = 0;j < numtlogs; j++)
        {
            ASSERT_TRUE(tlogIdsInSnap[j] == tlog_ids[i*numtlogs +j]);
        }
    }
    //   ASSERT_NO_THROW(fungi::File::unlink(sp_->getSnapshotsXMLPath()));
}

TEST_F(SnapshotPersistorTest, test5)
{
    const unsigned numsnaps = 10;
    const unsigned numtlogs = 50;

    std::vector<SnapshotName> snapshotnames;
    std::vector<TLogId> tlog_ids;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlog_ids.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlog_ids.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(SnapshotName(ss.str()));
        sp_->snapshot(snapshotnames[i]);
    }
    tlog_ids.push_back(sp_->getCurrentTLog());
//    sp_->saveToFile();

    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);
    sp_->deleteSnapshot(3);
    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);
    sp_->deleteSnapshot(7);
    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);
    sp_->deleteSnapshot(2);
    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);
    sp_->deleteSnapshot(0);
    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);
    sp_->deleteSnapshot(1);
    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);
    sp_->deleteSnapshot(9);
    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);
    sp_->deleteSnapshot(4);
    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);
    sp_->deleteSnapshot(8);
    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);
    sp_->deleteSnapshot(5);
    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);
    sp_->deleteSnapshot(6);
    testSnaps(*sp_, numsnaps, numtlogs, tlog_ids);

    ASSERT_THROW(sp_->deleteSnapshot(8),fungi::IOException);
}

TEST_F(SnapshotPersistorTest, test6)
{
    const unsigned numsnaps = 10;
    const unsigned numtlogs = 50;

    std::vector<SnapshotName> snapshotnames;
    std::vector<TLogId> tlog_ids;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlog_ids.push_back(sp_->getCurrentTLog());
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(SnapshotName(ss.str()));
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlog_ids.push_back(sp_->getCurrentTLog());
        }
        sp_->snapshot(snapshotnames[i]);

    }
//    sp_->saveToFile();
    for(unsigned i = 0; i < numsnaps; i++)
    {
        OrderedTLogIds tlog_ids;
        sp_->getTLogsTillSnapshot(i,
                                  tlog_ids);
        ASSERT_EQ(tlog_ids.size(), (i + 1) *numtlogs);
        tlog_ids.clear();

        sp_->getTLogsAfterSnapshot(i,
                                   tlog_ids);
        ASSERT_EQ(tlog_ids.size(),
                  (9-i)*numtlogs +1);
    }
}

TEST_F(SnapshotPersistorTest, test7)
{
    const unsigned numsnaps = 10;
    const unsigned numtlogs = 50;

    std::vector<SnapshotName> snapshotnames;
    std::vector<TLogId> tlog_ids;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlog_ids.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlog_ids.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(SnapshotName(ss.str()));
        sp_->snapshot(snapshotnames[i]);

    }
    sp_->deleteTLogsAndSnapshotsAfterSnapshot(5);
    ASSERT_FALSE(sp_->snapshotExists(6));
    ASSERT_TRUE(sp_->snapshotExists(5));
    OrderedTLogIds tlog_ids_in_snap;

    sp_->getCurrentTLogs(tlog_ids_in_snap);

    ASSERT_TRUE(tlog_ids_in_snap.size() == 1);
    tlog_ids_in_snap.clear();
    sp_->getTLogsInSnapshot(5,
                           tlog_ids_in_snap);
    ASSERT_EQ(tlog_ids_in_snap.size(),
              numtlogs);
    tlog_ids_in_snap.clear();
}

TEST_F(SnapshotPersistorTest, test8)
{
    const unsigned numsnaps = 5;
    const unsigned numtlogs = 10;

    std::vector<SnapshotName> snapshotnames;
    std::vector<TLogId> tlog_ids;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlog_ids.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlog_ids.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(SnapshotName(ss.str()));
        sp_->snapshot(snapshotnames[i]);

    }
//    sp_->saveToFile();

    // Move this to snapshotmanagementtest
//    OrderedTLogIds vec;
//     for(size_t i = 0; i < snapshotnames.size(); ++i)
//     {
// //         SnapshotNum num = sp_->getSnapshotNum(snapshotnames[i]);
// //        size_t maxEntries;
// //        sp_->increaseTLogCounter(numtlogs,vec);
// //        sp_->saveToFile();

//         EXPECT_TRUE(vec.size() == numtlogs);

//         for(size_t j = 0;j < tlog_ids.size(); ++j)
//         {
//             for(size_t k = 0; k < vec.size(); ++k)
//             {
//                 EXPECT_TRUE(tlog_ids[j] != vec[k]);
//             }
//         }
//         vec.clear();

//     }
}

TEST_F(SnapshotPersistorTest, test9)
{
    const unsigned numsnaps = 5;
    const unsigned numtlogs = 10;

    std::vector<SnapshotName> snapshotnames;
    std::vector<TLogId> tlog_ids;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlog_ids.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();

            tlog_ids.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(SnapshotName(ss.str()));
        sp_->snapshot(snapshotnames[i]);

    }
    //sp_->saveToFile();
    // move to snapshotmanagementtest
//     for(size_t i = 0; i < snapshotnames.size(); ++i)
//     {
//         OrderedTLogIds names;
//         sp_->increaseTLogCounter(1,names);
//         ASSERT_TRUE(names.size() == 1);

//         std::string name= names[0];
//         for(size_t j = 0;j < tlog_ids.size(); ++j)
//         {
//             EXPECT_TRUE(tlog_ids[j] != name);
//         }
//     }
}

TEST_F(SnapshotPersistorTest, test10)
{
    const unsigned numsnaps = 5;
    const unsigned numtlogs = 10;

    std::vector<SnapshotName> snapshotnames;
    std::vector<TLogId> tlog_ids;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlog_ids.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();

            tlog_ids.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(SnapshotName(ss.str()));
        sp_->snapshot(snapshotnames[i]);

    }
    // Y42 Move this to snapshotmanagementtest
    //    OrderedTLogIds t;
//     sp_->increaseTLogCounter(1,
//                             t);
//     ASSERT_TRUE(t.size() == 1);
//     EXPECT_TRUE(std::find(tlog_ids.begin(), tlog_ids.end(), t[0]) == tlog_ids.end());
//     OrderedTLogIds tlogsInSnap4;
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
        sp_->snapshot(SnapshotName(ss.str()));
        nums.push_back(sp_->getSnapshotNum(SnapshotName(ss.str())));
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
    sp_->snapshot(SnapshotName("snap1"));
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

    std::vector<SnapshotName> snapshotnames;
    std::vector<TLogId> tlog_ids;

    for(unsigned i = 0; i < numsnaps; i++)
    {
        tlog_ids.push_back(sp_->getCurrentTLog());
        for(unsigned j = 0; j < numtlogs-1; j++)
        {
            sp_->newTLog();
            tlog_ids.push_back(sp_->getCurrentTLog());
        }
        std::stringstream ss;
        ss << "snapshot_" << i;
        snapshotnames.push_back(SnapshotName(ss.str()));
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
    std::vector<TLogId> tlog_ids(num_tlogs);

    for (size_t i = 0; i < num_tlogs; ++i)
    {
        tlog_ids[i] = boost::lexical_cast<TLogId>(sp_->getCurrentTLog());

        if (i == snap_idx)
        {
            sp_->snapshot(SnapshotName("snapper"));
        }
        else
        {
            sp_->newTLog();
        }
    }

    {
        const TLogId& tid = tlog_ids[snap_idx - 1];
        EXPECT_THROW(sp_->setTLogWrittenToBackend(tid),
                     std::exception);
        EXPECT_FALSE(sp_->isTLogWrittenToBackend(tid));
    }

    for (size_t i = 0; i < snap_idx; ++i)
    {
        EXPECT_NO_THROW(sp_->setTLogWrittenToBackend(tlog_ids[i]));
    }

    {
        const TLogId& tid = tlog_ids[snap_idx + 2];
        EXPECT_THROW(sp_->setTLogWrittenToBackend(tid),
                     std::exception);
        EXPECT_FALSE(sp_->isTLogWrittenToBackend(tid));
    }

    for (size_t i = 0; i < num_tlogs; ++i)
    {
        EXPECT_NO_THROW(sp_->setTLogWrittenToBackend(tlog_ids[i]));
    }

    {
        OrderedTLogIds tlog_ids2;
        sp_->getTLogsWrittenToBackend(tlog_ids2);
        size_t i = 0;

        for (const auto& id : tlog_ids)
        {
            EXPECT_EQ(tlog_ids2[i],
                      id);
            ++i;
        }

        tlog_ids2.clear();
        sp_->getTLogsNotWrittenToBackend(tlog_ids2);
        EXPECT_EQ(1U,
                  tlog_ids2.size());
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
    const SnapshotName s("snapshot");
    sp_->snapshot(s);

    const Snapshot snap(sp_->getSnapshot(s));
    EXPECT_EQ(s, snap.getName());
    EXPECT_TRUE(snap.metadata().empty());
}

TEST_F(SnapshotPersistorTest, metadata)
{
    const SnapshotName s("pansshot");
    const SnapshotMetaData m(s.begin(), s.end());
    sp_->snapshot(s, m);

    const Snapshot snap(sp_->getSnapshot(s));
    EXPECT_EQ(s, snap.getName());
    EXPECT_TRUE(m == snap.metadata());
}

TEST_F(SnapshotPersistorTest, excessive_metadata)
{
    const SnapshotName s("tansposh");
    const SnapshotMetaData m(SnapshotPersistor::max_snapshot_metadata_size + 1, 'x');
    EXPECT_THROW(sp_->snapshot(s, m),
                 std::exception);
}

TEST_F(SnapshotPersistorTest, persistent_metadata)
{
    const SnapshotName s("antshops");
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

TEST_F(SnapshotPersistorTest, synced_to_backend)
{
    const TLogId tlog(boost::lexical_cast<TLogId>(sp_->getCurrentTLog()));
    // Does the SnapshotPersistor even care if the current TLog is marked as being
    // on the backend? Should it care?
    sp_->newTLog();

    EXPECT_FALSE(sp_->isTLogWrittenToBackend(tlog));
    sp_->setTLogWrittenToBackend(tlog);
    EXPECT_TRUE(sp_->isTLogWrittenToBackend(tlog));
}

}

// Local Variables: **
// mode: c++ **
// End: **
