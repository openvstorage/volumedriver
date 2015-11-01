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
    Volume* v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    const std::string pattern1("Frederik");

    writeToVolume(v, 0, 4096, pattern1);
    waitForThisBackendWrite(v);
    v->createSnapshot("snap1");

    const std::string pattern2("Frederik");

    writeToVolume(v, 0, 4096, pattern2);
    waitForThisBackendWrite(v);
    v->createSnapshot("snap2");


    const std::string pattern3("Arne");

    writeToVolume(v, 0, 4096, pattern3);
    waitForThisBackendWrite(v);
    v->createSnapshot("snap3");

    const std::string pattern4("Bart");

    writeToVolume(v, 0, 4096, pattern4);
    waitForThisBackendWrite(v);
    v->createSnapshot("snap4");

    const std::string pattern5("Wouter");
    writeToVolume(v, 0, 4096, pattern5);

    checkVolume(v,0,4096,pattern5);
    waitForThisBackendWrite(v);

    EXPECT_NO_THROW(restoreSnapshot(v,"snap4"));

    checkVolume(v,0,4096,pattern4);
    writeToVolume(v, 0, 4096, "Bollocks");
    waitForThisBackendWrite(v);
    v->createSnapshot("snapper");
    waitForThisBackendWrite(v);

    EXPECT_NO_THROW(restoreSnapshot(v,"snap3"));

    checkVolume(v,0,4096,pattern3);
    writeToVolume(v, 0, 4096, "Bollocks");
    waitForThisBackendWrite(v);
    v->createSnapshot("snapper");
    waitForThisBackendWrite(v);

    EXPECT_NO_THROW(restoreSnapshot(v,"snap2"));

    checkVolume(v,0,4096,pattern2);
    writeToVolume(v, 0, 4096, "Bollocks");
    waitForThisBackendWrite(v);
    v->createSnapshot("snapper");
    waitForThisBackendWrite(v);

    EXPECT_NO_THROW(restoreSnapshot(v,"snap1"));

    checkVolume(v,0,4096,pattern1);
    writeToVolume(v, 0, 4096, "Bollocks");
    waitForThisBackendWrite(v);
    v->createSnapshot("snapper");
    waitForThisBackendWrite(v);
    checkCurrentBackendSize(v);
}

TEST_P(SnapshotRestoreTest, NoSCOGap)
{
    auto ns_ptr = make_random_namespace();
    Volume* v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    const std::string pattern1("Frederik");

    writeToVolume(v, 0, 4096, pattern1);
    ASSERT_TRUE(getCurrentSCO(v)->getSCO().number() == 1);

    waitForThisBackendWrite(v);
    v->createSnapshot("snap1");

    const std::string pattern2("Frederik");

    writeToVolume(v, 0, 4096, pattern2);
    ASSERT_TRUE(getCurrentSCO(v)->getSCO().number() == 2);
    waitForThisBackendWrite(v);
    v->createSnapshot("snap2");

    const std::string pattern3("Arne");

    writeToVolume(v, 0, 4096, pattern3);
    ASSERT_TRUE(getCurrentSCO(v)->getSCO().number() == 3);
    waitForThisBackendWrite(v);
    v->createSnapshot("snap3");

    const std::string pattern4("Bart");

    writeToVolume(v, 0, 4096, pattern4);
    ASSERT_TRUE(getCurrentSCO(v)->getSCO().number() == 4);
    waitForThisBackendWrite(v);
    v->createSnapshot("snap4");

    const std::string pattern5("Wouter");
    writeToVolume(v, 0, 4096, pattern5);
    ASSERT_TRUE(getCurrentSCO(v)->getSCO().number() == 5);

    waitForThisBackendWrite(v);
    EXPECT_NO_THROW(restoreSnapshot(v,"snap4"));

    checkVolume(v,0,4096,pattern4);
    writeToVolume(v, 0, 4096, "Bollocks");
    ASSERT_TRUE(getCurrentSCO(v)->getSCO().number() == 5);
    waitForThisBackendWrite(v);
    v->createSnapshot("snapper");
    waitForThisBackendWrite(v);

    EXPECT_NO_THROW(restoreSnapshot(v,"snap3"));

    checkVolume(v,0,4096,pattern3);
    writeToVolume(v, 0, 4096, "Bollocks");
    ASSERT_TRUE(getCurrentSCO(v)->getSCO().number() == 4);
    waitForThisBackendWrite(v);
    v->createSnapshot("snapper");
    waitForThisBackendWrite(v);

    EXPECT_NO_THROW(restoreSnapshot(v,"snap2"));

    checkVolume(v,0,4096,pattern2);
    writeToVolume(v, 0, 4096, "Bollocks");
    ASSERT_TRUE(getCurrentSCO(v)->getSCO().number() == 3);
    waitForThisBackendWrite(v);
    v->createSnapshot("snapper");
    waitForThisBackendWrite(v);

    EXPECT_NO_THROW(restoreSnapshot(v,"snap1"));

    checkVolume(v,0,4096,pattern1);
    writeToVolume(v, 0, 4096, "Bollocks");
    ASSERT_TRUE(getCurrentSCO(v)->getSCO().number() == 2);
    waitForThisBackendWrite(v);
    v->createSnapshot("snapper");
    waitForThisBackendWrite(v);
    checkCurrentBackendSize(v);
}

TEST_P(SnapshotRestoreTest, RestoreAndWriteAgain1)
{
    auto ns_ptr = make_random_namespace();
    Volume* v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns(),
                          VolumeSize(1 << 26),
                          SCOMultiplier(1));

    const std::string pattern("e-manual");

    v->createSnapshot("snap1");
    waitForThisBackendWrite(v);

    writeToVolume(v, 0, 5 * 4096, pattern);
    waitForThisBackendWrite(v);

    restoreSnapshot(v,"snap1");

    writeToVolume(v, 0, 4*4096, pattern);
    waitForThisBackendWrite(v);
    checkCurrentBackendSize(v);
}

TEST_P(SnapshotRestoreTest, RestoreAndWriteAgain2)
{
    auto ns_ptr = make_random_namespace();
    Volume* v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns(),
                          VolumeSize(1 << 26),
                          SCOMultiplier(1));

    const std::string pattern("e-manual");

    v->createSnapshot("snap1");
    waitForThisBackendWrite(v);

    writeToVolume(v, 0, 5 * 4096, pattern);
    v->createSnapshot("snap2");
    waitForThisBackendWrite(v);

    restoreSnapshot(v,"snap1");

    writeToVolume(v, 0, 10*4096, pattern);
    waitForThisBackendWrite(v);
    checkCurrentBackendSize(v);
}

TEST_P(SnapshotRestoreTest, TestFailOver)
{
    auto foc_ctx(start_one_foc());
    auto ns_ptr = make_random_namespace();
    Volume* v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns(),
                          VolumeSize((1 << 18) * 512),
                          SCOMultiplier(1));

    v->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    VolumeConfig cfg = v->get_config();
    v->createSnapshot("snap0");

    for(int i = 0; i < 5; ++i)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "a");
    }


    waitForThisBackendWrite(v);
    v->restoreSnapshot("snap0");

    for(int i = 0; i < 7; ++i)
    {
        writeToVolume(v,
                      8,
                      4096,
                      "d");
    }

    flushFailOverCache(v);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    Volume* v1 = 0;
    v1 = getVolume(VolumeId("volume1"));
    ASSERT_FALSE(v1);
    restartVolume(cfg);
    v1 = getVolume(VolumeId("volume1"));

    ASSERT_TRUE(v1);
    checkVolume(v1,0,4096, "\0");
    checkVolume(v1,8,4096, "d");
    checkCurrentBackendSize(v1);
}

TEST_P(SnapshotRestoreTest, HaltOnError)
{
    auto ns_ptr = make_random_namespace();
    Volume* v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    const std::string pattern1("blah");

    const std::string tlog(v->getSnapshotManagement().getCurrentTLogName());

    writeToVolume(v, 0, 4096, pattern1);
    v->createSnapshot("snap1");
    waitForThisBackendWrite(v);

    EXPECT_THROW(restoreSnapshot(v, "snap42"),
                 std::exception);
    EXPECT_FALSE(v->is_halted());

    v->getBackendInterface()->remove(tlog);
    EXPECT_THROW(restoreSnapshot(v, "snap1"),
                 std::exception);
    EXPECT_TRUE(v->is_halted());
}

TEST_P(SnapshotRestoreTest, snapshot_restoration_on_a_clone)
{
    const auto wrns_parent(make_random_namespace());
    const std::string parent_name(wrns_parent->ns().str());

    Volume* parent = newVolume(VolumeId(parent_name),
                               be::Namespace(parent_name));

    const std::string pattern1("before-parent-snapshot");

    writeToVolume(parent,
                  0,
                  parent->getClusterSize(),
                  pattern1);

    const std::string parent_snap("parent-snapshot");
    parent->createSnapshot(parent_snap);

    waitForThisBackendWrite(parent);

    const auto wrns_clone(make_random_namespace());
    const std::string clone_name(wrns_clone->ns().str());

    Volume* clone = createClone(clone_name,
                                be::Namespace(clone_name),
                                be::Namespace(parent_name),
                                parent_snap);

    const std::string pattern2("before-clone-snapshot");

    writeToVolume(clone,
                  0,
                  clone->getClusterSize(),
                  pattern2);

    const std::string clone_snap("clone-snapshot");
    clone->createSnapshot(clone_snap);

    waitForThisBackendWrite(clone);

    const std::string pattern3("after-clone-snapshot");

    writeToVolume(clone,
                  0,
                  clone->getClusterSize(),
                  pattern3);

    checkVolume(clone,
                0,
                clone->getClusterSize(),
                pattern3);

    restoreSnapshot(clone,
                    clone_snap);

    checkVolume(clone,
                0,
                clone->getClusterSize(),
                pattern2);
}

INSTANTIATE_TEST(SnapshotRestoreTest);

}

// Local Variables: **
// mode: c++ **
// End: **
