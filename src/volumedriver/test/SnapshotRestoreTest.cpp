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

#include <youtils/Assert.h>

namespace volumedriver
{
class SnapshotRestoreTest
    : public VolManagerTestSetup
{
public:
    SnapshotRestoreTest()
        : VolManagerTestSetup("SnapshotRestoreTest")
    {}
};

TEST_P(SnapshotRestoreTest, SimpleRestore)
{
    auto ns_ptr = make_random_namespace();
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    const std::string pattern1("Frederik");

    writeToVolume(*v, Lba(0), 4096, pattern1);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snap1"));

    const std::string pattern2("Frederik");

    writeToVolume(*v, Lba(0), 4096, pattern2);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snap2"));


    const std::string pattern3("Arne");

    writeToVolume(*v, Lba(0), 4096, pattern3);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snap3"));

    const std::string pattern4("Bart");

    writeToVolume(*v, Lba(0), 4096, pattern4);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snap4"));

    const std::string pattern5("Wouter");
    writeToVolume(*v, Lba(0), 4096, pattern5);

    checkVolume(*v, Lba(0),4096,pattern5);
    waitForThisBackendWrite(*v);

    EXPECT_NO_THROW(restoreSnapshot(*v,
                                    "snap4"));

    checkVolume(*v, Lba(0),4096,pattern4);
    writeToVolume(*v, Lba(0), 4096, "Bollocks");
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snapper"));
    waitForThisBackendWrite(*v);

    EXPECT_NO_THROW(restoreSnapshot(*v,
                                    "snap3"));

    checkVolume(*v, Lba(0),4096,pattern3);
    writeToVolume(*v, Lba(0), 4096, "Bollocks");
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snapper"));
    waitForThisBackendWrite(*v);

    EXPECT_NO_THROW(restoreSnapshot(*v,
                                    "snap2"));

    checkVolume(*v, Lba(0),4096,pattern2);
    writeToVolume(*v, Lba(0), 4096, "Bollocks");
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snapper"));
    waitForThisBackendWrite(*v);

    EXPECT_NO_THROW(restoreSnapshot(*v,
                                    "snap1"));

    checkVolume(*v, Lba(0),4096,pattern1);
    writeToVolume(*v, Lba(0), 4096, "Bollocks");
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snapper"));
    waitForThisBackendWrite(*v);
    checkCurrentBackendSize(*v);
}

TEST_P(SnapshotRestoreTest, NoSCOGap)
{
    auto ns_ptr = make_random_namespace();
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    const std::string pattern1("Frederik");

    writeToVolume(*v, Lba(0), 4096, pattern1);
    ASSERT_TRUE(getCurrentSCO(*v)->getSCO().number() == 1);

    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snap1"));

    const std::string pattern2("Frederik");

    writeToVolume(*v, Lba(0), 4096, pattern2);
    ASSERT_TRUE(getCurrentSCO(*v)->getSCO().number() == 2);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snap2"));

    const std::string pattern3("Arne");

    writeToVolume(*v, Lba(0), 4096, pattern3);
    ASSERT_TRUE(getCurrentSCO(*v)->getSCO().number() == 3);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snap3"));

    const std::string pattern4("Bart");

    writeToVolume(*v, Lba(0), 4096, pattern4);
    ASSERT_TRUE(getCurrentSCO(*v)->getSCO().number() == 4);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snap4"));

    const std::string pattern5("Wouter");
    writeToVolume(*v, Lba(0), 4096, pattern5);
    ASSERT_TRUE(getCurrentSCO(*v)->getSCO().number() == 5);

    waitForThisBackendWrite(*v);
    EXPECT_NO_THROW(restoreSnapshot(*v,"snap4"));

    checkVolume(*v, Lba(0),4096,pattern4);
    writeToVolume(*v, Lba(0), 4096, "Bollocks");
    ASSERT_TRUE(getCurrentSCO(*v)->getSCO().number() == 5);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snapper"));
    waitForThisBackendWrite(*v);

    EXPECT_NO_THROW(restoreSnapshot(*v, "snap3"));

    checkVolume(*v, Lba(0),4096,pattern3);
    writeToVolume(*v, Lba(0), 4096, "Bollocks");
    ASSERT_TRUE(getCurrentSCO(*v)->getSCO().number() == 4);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snapper"));
    waitForThisBackendWrite(*v);

    EXPECT_NO_THROW(restoreSnapshot(*v, "snap2"));

    checkVolume(*v, Lba(0),4096,pattern2);
    writeToVolume(*v, Lba(0), 4096, "Bollocks");
    ASSERT_TRUE(getCurrentSCO(*v)->getSCO().number() == 3);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snapper"));
    waitForThisBackendWrite(*v);

    EXPECT_NO_THROW(restoreSnapshot(*v,"snap1"));

    checkVolume(*v, Lba(0),4096,pattern1);
    writeToVolume(*v, Lba(0), 4096, "Bollocks");
    ASSERT_TRUE(getCurrentSCO(*v)->getSCO().number() == 2);
    waitForThisBackendWrite(*v);
    v->createSnapshot(SnapshotName("snapper"));
    waitForThisBackendWrite(*v);
    checkCurrentBackendSize(*v);
}

TEST_P(SnapshotRestoreTest, RestoreAndWriteAgain1)
{
    auto ns_ptr = make_random_namespace();
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns(),
                          VolumeSize(1 << 26),
                          SCOMultiplier(1));

    const std::string pattern("e-manual");

    v->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(*v);

    writeToVolume(*v, Lba(0), 5 * 4096, pattern);
    waitForThisBackendWrite(*v);

    restoreSnapshot(*v,"snap1");

    writeToVolume(*v, Lba(0), 4*4096, pattern);
    waitForThisBackendWrite(*v);
    checkCurrentBackendSize(*v);
}

TEST_P(SnapshotRestoreTest, RestoreAndWriteAgain2)
{
    auto ns_ptr = make_random_namespace();
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns(),
                          VolumeSize(1 << 26),
                          SCOMultiplier(1));

    const std::string pattern("e-manual");

    v->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(*v);

    writeToVolume(*v, Lba(0), 5 * 4096, pattern);
    v->createSnapshot(SnapshotName("snap2"));
    waitForThisBackendWrite(*v);

    restoreSnapshot(*v,"snap1");

    writeToVolume(*v, Lba(0), 10*4096, pattern);
    waitForThisBackendWrite(*v);
    checkCurrentBackendSize(*v);
}

TEST_P(SnapshotRestoreTest, TestFailOver)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns(),
                          VolumeSize((1 << 18) * 512),
                          SCOMultiplier(1));

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    VolumeConfig cfg = v->get_config();
    v->createSnapshot(SnapshotName("snap0"));

    for(int i = 0; i < 5; ++i)
    {
        writeToVolume(*v,
                      Lba(0),
                      4096,
                      "a");
    }


    waitForThisBackendWrite(*v);
    v->restoreSnapshot(SnapshotName("snap0"));

    for(int i = 0; i < 7; ++i)
    {
        writeToVolume(*v,
                      Lba(8),
                      4096,
                      "d");
    }

    flushFailOverCache(*v);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    SharedVolumePtr v1 = 0;
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v1);
    restartVolume(cfg);
    v1 = getVolume(VolumeId("volume1"));

    ASSERT_TRUE(v1 != nullptr);
    checkVolume(*v1, Lba(0), 4096, "\0");
    checkVolume(*v1, Lba(8), 4096, "d");
    checkCurrentBackendSize(*v1);
}

TEST_P(SnapshotRestoreTest, HaltOnError)
{
    auto ns_ptr = make_random_namespace();
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    const std::string pattern1("blah");

    const TLogId tlog_id(v->getSnapshotManagement().getCurrentTLogId());

    writeToVolume(*v, Lba(0), 4096, pattern1);
    v->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(*v);

    EXPECT_THROW(restoreSnapshot(*v, "snap42"),
                 std::exception);
    EXPECT_FALSE(v->is_halted());

    v->getBackendInterface()->remove(boost::lexical_cast<std::string>(tlog_id));
    EXPECT_THROW(restoreSnapshot(*v, "snap1"),
                 std::exception);
    EXPECT_TRUE(v->is_halted());
}

TEST_P(SnapshotRestoreTest, snapshot_restoration_on_a_clone)
{
    const auto wrns_parent(make_random_namespace());
    const std::string parent_name(wrns_parent->ns().str());

    SharedVolumePtr parent = newVolume(VolumeId(parent_name),
                               be::Namespace(parent_name));

    const std::string pattern1("before-parent-snapshot");

    writeToVolume(*parent,
                  Lba(0),
                  parent->getClusterSize(),
                  pattern1);

    const SnapshotName parent_snap("parent-snapshot");
    parent->createSnapshot(parent_snap);

    waitForThisBackendWrite(*parent);

    const auto wrns_clone(make_random_namespace());
    const std::string clone_name(wrns_clone->ns().str());

    SharedVolumePtr clone = createClone(clone_name,
                                be::Namespace(clone_name),
                                be::Namespace(parent_name),
                                parent_snap);

    const std::string pattern2("before-clone-snapshot");

    writeToVolume(*clone,
                  Lba(0),
                  clone->getClusterSize(),
                  pattern2);

    const SnapshotName clone_snap("clone-snapshot");
    clone->createSnapshot(clone_snap);

    waitForThisBackendWrite(*clone);

    const std::string pattern3("after-clone-snapshot");

    writeToVolume(*clone,
                  Lba(0),
                  clone->getClusterSize(),
                  pattern3);

    checkVolume(*clone,
                Lba(0),
                clone->getClusterSize(),
                pattern3);

    restoreSnapshot(*clone,
                    clone_snap);

    checkVolume(*clone,
                Lba(0),
                clone->getClusterSize(),
                pattern2);
}

INSTANTIATE_TEST(SnapshotRestoreTest);

}

// Local Variables: **
// mode: c++ **
// End: **
