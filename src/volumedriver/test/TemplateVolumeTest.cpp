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

#include "../Api.h"
#include "../ScrubReply.h"

#include <youtils/FileUtils.h>

namespace volumedrivertest
{

namespace yt = youtils;
namespace fs = boost::filesystem;

using namespace volumedriver;

class TemplateVolumeTest
    : public VolManagerTestSetup
{
public:
    TemplateVolumeTest()
        : VolManagerTestSetup("TemplateVolumeTest")
    {}

    void
    set_as_template(const VolumeId& vid)
    {
        fungi::ScopedLock l(api::getManagementMutex());
        VolManager::get()->setAsTemplate(vid);
    }
};

TEST_P(TemplateVolumeTest, forbidden_actions)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns1 = ns_ptr->ns();
    Volume* v = newVolume(vid1,
                          ns1);

    EXPECT_NO_THROW(set_as_template(vid1));

    EXPECT_TRUE(T(v->isVolumeTemplate()));

    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                              "blah"),
                 VolumeIsTemplateException);
    EXPECT_THROW(v->createSnapshot(SnapshotName("snap1")),
                 VolumeIsTemplateException);

    std::list<SnapshotName> snapshots;
    v->listSnapshots(snapshots);
    ASSERT_EQ(1U, snapshots.size());
    EXPECT_THROW(v->deleteSnapshot(snapshots.front()),
                 VolumeIsTemplateException);

    std::vector<std::string> w;
    EXPECT_THROW(v->getScrubbingWork(w,
                                     boost::none,
                                     boost::none),
                 VolumeIsTemplateException);

    const scrubbing::ScrubReply scrub_rep(v->getNamespace(),
                                          SnapshotName("snap"),
                                          "scrub_res");

    EXPECT_THROW(v->applyScrubbingWork(scrub_rep),
                 VolumeIsTemplateException);
}

TEST_P(TemplateVolumeTest, set_template_no_data_no_snapshot)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns1 = ns_ptr->ns();
    Volume* v = newVolume(vid1,
                          ns1);

    EXPECT_NO_THROW(set_as_template(vid1));
    EXPECT_TRUE(T(v->isVolumeTemplate()));

    std::list<SnapshotName> snapshots;
    v->listSnapshots(snapshots);
    EXPECT_EQ(1U, snapshots.size());

    checkVolume(v, 0, 4096, std::string(1, 0));
}

TEST_P(TemplateVolumeTest, idempotency)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns1 = ns_ptr->ns();
    Volume* v = newVolume(vid1,
                          ns1);
    writeToVolume(v,
                  0,
                  4096,
                  "blah");

    EXPECT_NO_THROW(set_as_template(vid1));
    EXPECT_TRUE(T(v->isVolumeTemplate()));
    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "blah"),
                 VolumeIsTemplateException);

    EXPECT_NO_THROW(set_as_template(vid1));
    EXPECT_TRUE(T(v->isVolumeTemplate()));
    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "blah"),
                 VolumeIsTemplateException);
    {
        fungi::ScopedLock l(api::getManagementMutex());
        EXPECT_NO_THROW(VolManager::get()->setAsTemplate(vid1));
    }

    EXPECT_TRUE(T(v->isVolumeTemplate()));
    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "blah"),
                 VolumeIsTemplateException);

}

TEST_P(TemplateVolumeTest, set_template_data_no_snapshot)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns1 = ns_ptr->ns();
    Volume* v = newVolume(vid1,
                          ns1);
    writeToVolume(v,
                  0,
                  4096,
                  "blah");

    EXPECT_NO_THROW(set_as_template(vid1));
    EXPECT_TRUE(T(v->isVolumeTemplate()));

    std::list<SnapshotName> snapshots;
    v->listSnapshots(snapshots);
    EXPECT_EQ(1U, snapshots.size());
    checkVolume(v, 0, 4096, "blah");
}

TEST_P(TemplateVolumeTest, set_template_with_last_snapshot)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns1 = ns_ptr->ns();
    Volume* v = newVolume(vid1,
                          ns1);
    writeToVolume(v,
                  0,
                  4096,
                  "blah1");

    const SnapshotName first_snap("first_snap");

    v->createSnapshot(first_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah2");
    waitForThisBackendWrite(v);
    const SnapshotName second_snap("second_snap");

    v->createSnapshot(second_snap);

    EXPECT_NO_THROW(set_as_template(vid1));
    EXPECT_TRUE(T(v->isVolumeTemplate()));

    std::list<SnapshotName> snapshots;
    v->listSnapshots(snapshots);
    EXPECT_EQ(1U, snapshots.size());
    EXPECT_EQ(snapshots.front(), second_snap);

    checkVolume(v, 0, 4096, "blah2");
}

TEST_P(TemplateVolumeTest, set_template_with_data_beyond_last_snapshot)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns1 = ns_ptr->ns();
    Volume* v = newVolume(vid1,
                          ns1);
    writeToVolume(v,
                  0,
                  4096,
                  "blaha");
    const SnapshotName first_snap("first_snap");

    v->createSnapshot(first_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah2");
    waitForThisBackendWrite(v);
    const SnapshotName second_snap("second_snap");

    v->createSnapshot(second_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah3");
    waitForThisBackendWrite(v);
    const SnapshotName third_snap("third_snap");

    v->createSnapshot(third_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah4");

    EXPECT_NO_THROW(set_as_template(vid1));

    std::list<SnapshotName> snapshots;
    v->listSnapshots(snapshots);
    EXPECT_EQ(1U, snapshots.size());
    EXPECT_NE(snapshots.front(), first_snap);
    EXPECT_NE(snapshots.front(), second_snap);
    EXPECT_NE(snapshots.front(), third_snap);
    checkVolume(v, 0, 4096, "blah4");
}

TEST_P(TemplateVolumeTest, localrestart)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns1 = ns_ptr->ns();
    Volume* v = newVolume(vid1,
                          ns1);
    writeToVolume(v,
                  0,
                  4096,
                  "blaha");
    const SnapshotName first_snap("first_snap");

    v->createSnapshot(first_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah2");
    waitForThisBackendWrite(v);
    const SnapshotName second_snap("second_snap");

    v->createSnapshot(second_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah3");

    waitForThisBackendWrite(v);
    const SnapshotName third_snap("third_snap");

    v->createSnapshot(third_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah4");

    EXPECT_NO_THROW(set_as_template(vid1));

    EXPECT_TRUE(T(v->isVolumeTemplate()));
    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "blah"),
                 VolumeIsTemplateException);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    v = 0;

    ASSERT_NO_THROW(v = localRestart(ns1));
    ASSERT_TRUE(v);

    EXPECT_TRUE(T(v->isVolumeTemplate()));
    checkVolume(v, 0, 4096, "blah4");
    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "blah"),
                 VolumeIsTemplateException);

    EXPECT_NO_THROW(set_as_template(vid1));

    EXPECT_TRUE(T(v->isVolumeTemplate()));
    checkVolume(v, 0, 4096, "blah4");
    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "blah"),
                 VolumeIsTemplateException);

    // cf. OVS-1099

}

TEST_P(TemplateVolumeTest, backend_restart)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns1 = ns_ptr->ns();
    Volume* v = newVolume(vid1,
                          ns1);
    writeToVolume(v,
                  0,
                  4096,
                  "blaha");
    const SnapshotName first_snap("first_snap");

    v->createSnapshot(first_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah2");
    waitForThisBackendWrite(v);
    const SnapshotName second_snap("second_snap");

    v->createSnapshot(second_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah3");
    waitForThisBackendWrite(v);
    const SnapshotName third_snap("third_snap");

    v->createSnapshot(third_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah4");

    EXPECT_NO_THROW(set_as_template(vid1));

    EXPECT_TRUE(T(v->isVolumeTemplate()));
    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "blah"),
                 VolumeIsTemplateException);
    VolumeConfig cfg = v->get_config();
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    v = 0;

    ASSERT_NO_THROW(restartVolume(cfg));
    v = getVolume(vid1);

    ASSERT_TRUE(v);

    EXPECT_TRUE(T(v->isVolumeTemplate()));
    checkVolume(v, 0, 4096, "blah4");
    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "blah"),
                 VolumeIsTemplateException);

    EXPECT_NO_THROW(set_as_template(vid1));

    EXPECT_TRUE(T(v->isVolumeTemplate()));
    checkVolume(v, 0, 4096, "blah4");
    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "blah"),
                 VolumeIsTemplateException);


}

TEST_P(TemplateVolumeTest, cloneTemplatedVolume)
{
    VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns1 = ns_ptr->ns();

    Volume* v = newVolume(vid1,
                          ns1);
    writeToVolume(v,
                  0,
                  4096,
                  "blaha");
    const SnapshotName first_snap("first_snap");

    v->createSnapshot(first_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah2");
    waitForThisBackendWrite(v);
    const SnapshotName second_snap("second_snap");

    v->createSnapshot(second_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah3");
    waitForThisBackendWrite(v);
    const SnapshotName third_snap("third_snap");

    v->createSnapshot(third_snap);
    writeToVolume(v,
                  0,
                  4096,
                  "blah4");

    EXPECT_NO_THROW(set_as_template(vid1));

    EXPECT_TRUE(T(v->isVolumeTemplate()));
    EXPECT_THROW(writeToVolume(v,
                               0,
                               4096,
                               "blah"),
                 VolumeIsTemplateException);
    VolumeConfig cfg  = v->get_config();
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    VolumeId c1ID("clone1");
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();


    Volume* c1 = 0;

    ASSERT_NO_THROW(c1 = createClone(c1ID,
                                     ns2,
                                     ns1,
                                     boost::none));
    checkVolume(c1, 0, 4096, "blah4");
    EXPECT_FALSE(T(c1->isVolumeTemplate()));
}

// cf. OVS-1099: snapshots.xml wasn't written out locally after removing
// all but the last snapshot, leading to an invasion of zombie snapshots
// after restart after a crash.
TEST_P(TemplateVolumeTest, return_of_the_zombie_snapshots)
{
    const VolumeId vid1("volume1");
    auto ns_ptr = make_random_namespace();
    const Namespace& ns1 = ns_ptr->ns();

    Volume* v = newVolume(vid1,
                          ns1);

    const SnapshotName first_snap("first_snap");
    v->createSnapshot(first_snap);
    waitForThisBackendWrite(v);

    const SnapshotName last_snap("last_snap");
    v->createSnapshot(last_snap);
    waitForThisBackendWrite(v);

    {
        std::list<SnapshotName> snapshots;
        v->listSnapshots(snapshots);
        EXPECT_EQ(2U, snapshots.size());
    }

    EXPECT_NO_THROW(set_as_template(vid1));

    {
        std::list<SnapshotName> snapshots;
        v->listSnapshots(snapshots);
        EXPECT_EQ(1U, snapshots.size());
        EXPECT_EQ(last_snap, snapshots.front());
    }

    // Simulate a crash by setting the local snapshots.xml aside as the orderly shutdown
    // writes it out later on.
    const fs::path snap_path(VolManager::get()->getSnapshotsPath(v));
    const fs::path snap_backup_path(snap_path.string() + ".BAK");

    yt::FileUtils::safe_copy(snap_path,
                             snap_backup_path);

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    v = 0;

    yt::FileUtils::safe_copy(snap_backup_path,
                             snap_path);

    ASSERT_NO_THROW(v = localRestart(ns1));
    ASSERT_TRUE(v);

    {
        std::list<SnapshotName> snapshots;
        v->listSnapshots(snapshots);
        EXPECT_EQ(1U, snapshots.size());
        EXPECT_EQ(last_snap, snapshots.front());
    }
}

INSTANTIATE_TEST(TemplateVolumeTest);

}
