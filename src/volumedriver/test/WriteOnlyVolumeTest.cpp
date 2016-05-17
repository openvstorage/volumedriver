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

namespace volumedrivertest
{
using namespace volumedriver;

class WriteOnlyVolumeTest
    : public VolManagerTestSetup
{
public:
    WriteOnlyVolumeTest()
        : VolManagerTestSetup("WriteOnlyVolumeTest")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }
};

TEST_P(WriteOnlyVolumeTest, test1)
{
    VolumeId vid("volume1");
    std::unique_ptr<WithRandomNamespace> ns_ptr = make_random_namespace();

    const Namespace& ns = ns_ptr->ns();


    WriteOnlyVolume* v = newWriteOnlyVolume(vid,
                                            ns,
                                            VolumeConfig::WanBackupVolumeRole::WanBackupNormal);
    ASSERT_TRUE(v);
    const VolumeConfig vCfg = v->get_config();

    const std::string pattern("blah");
    const std::string pattern2("halb");
    writeToVolume(*v, 0, 4096, pattern);
    v->scheduleBackendSync();
    waitForThisBackendWrite(*v);

    destroyVolume(v,
                  RemoveVolumeCompletely::F);
    v = 0;

    restartVolume(vCfg);

    SharedVolumePtr vol = getVolume(vid);

    ASSERT_TRUE(vol != nullptr);

    checkVolume(*vol, 0, 4096, pattern);

    destroyVolume(vol,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    vol = 0;

    setVolumeRole(ns, VolumeConfig::WanBackupVolumeRole::WanBackupBase);
    v = restartWriteOnlyVolume(vCfg);
    writeToVolume(*v, 8, 4096, pattern2);
    v->scheduleBackendSync();
    waitForThisBackendWrite(*v);

    destroyVolume(v,
                  RemoveVolumeCompletely::F);
    v = 0;

    setVolumeRole(ns, VolumeConfig::WanBackupVolumeRole::WanBackupNormal);
    restartVolume(vCfg);

    vol = getVolume(vid);

    ASSERT_TRUE(vol != nullptr);

    checkVolume(*vol, 0, 4096, pattern);
    checkVolume(*vol, 8, 4096, pattern2);
    destroyVolume(vol,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    vol = 0;
}

TEST_P(WriteOnlyVolumeTest, no_restart_from_incremental_volume)
{
    VolumeId vid("volume1");
    std::unique_ptr<WithRandomNamespace> ns_ptr = make_random_namespace();

    const Namespace& ns = ns_ptr->ns();
    WriteOnlyVolume* v = newWriteOnlyVolume(vid,
                                            ns,
                                            VolumeConfig::WanBackupVolumeRole::WanBackupIncremental);
    ASSERT_TRUE(v);
    const VolumeConfig vCfg = v->get_config();

    const std::string pattern("blah");
    const std::string pattern2("halb");
    writeToVolume(*v, 0, 4096, pattern);
    v->scheduleBackendSync();
    waitForThisBackendWrite(*v);

    destroyVolume(v,
                  RemoveVolumeCompletely::F);
    v = 0;

    ASSERT_THROW(restartVolume(vCfg), VolManager::VolumeDoesNotHaveCorrectRole);
}

TEST_P(WriteOnlyVolumeTest, no_clone_from_incremental)
{

    VolumeId vid("INCREdiblyMENTAL");
    std::unique_ptr<WithRandomNamespace> ns_ptr = make_random_namespace();

    const Namespace& ns = ns_ptr->ns();


    WriteOnlyVolume* wov =
        newWriteOnlyVolume(vid,
                           ns,
                           VolumeConfig::WanBackupVolumeRole::WanBackupIncremental);
    ASSERT_TRUE(wov != nullptr);

    const VolumeConfig cfg(wov->get_config());

    const std::string pattern("All work and no play makes Jack a dull boy.");
    writeToVolume(*wov, 0, 16384, pattern);

    const SnapshotName snap("snap");
    wov->createSnapshot(snap);
    waitForThisBackendWrite(*wov);

    destroyVolume(wov,
                  RemoveVolumeCompletely::F);

    std::string child("child");

    std::unique_ptr<WithRandomNamespace> child_ns_ptr = make_random_namespace();
    const Namespace& child_ns = child_ns_ptr->ns();

    EXPECT_THROW(createClone(child,
                             child_ns,
                             ns,
                             snap),
                 VolManager::VolumeDoesNotHaveCorrectRole);
}

TEST_P(WriteOnlyVolumeTest, no_restart_from_base_volume)
{
    VolumeId vid("volume1");

    std::unique_ptr<WithRandomNamespace> ns_ptr = make_random_namespace();

    const Namespace& ns = ns_ptr->ns();

    WriteOnlyVolume* v = newWriteOnlyVolume(vid,
                                            ns,
                                            VolumeConfig::WanBackupVolumeRole::WanBackupBase);
    ASSERT_TRUE(v);
    const VolumeConfig vCfg = v->get_config();

    const std::string pattern("blah");
    const std::string pattern2("halb");
    writeToVolume(*v, 0, 4096, pattern);
    v->scheduleBackendSync();
    waitForThisBackendWrite(*v);

    destroyVolume(v,
                  RemoveVolumeCompletely::F);
    v = 0;
    ASSERT_THROW(restartVolume(vCfg),
                 VolManager::VolumeDoesNotHaveCorrectRole);
}

TEST_P(WriteOnlyVolumeTest, test2)
{
    VolumeId vid("volume1");
    std::unique_ptr<WithRandomNamespace> ns_ptr = make_random_namespace();

    const Namespace& ns = ns_ptr->ns();


    WriteOnlyVolume* v = newWriteOnlyVolume(vid,
                                            ns,
                                            VolumeConfig::WanBackupVolumeRole::WanBackupNormal);
    ASSERT_TRUE(v);
    const VolumeConfig vCfg = v->get_config();

    const uint32_t num_snapshots = 32;

    for(unsigned i = 0; i < num_snapshots; ++i)
    {
        std::stringstream ss;
        ss << std::setfill('_') << std::setw(2) << i;
        writeToVolume(*v, 0, 4096, ss.str());
        v->createSnapshot(SnapshotName(ss.str()));
        waitForThisBackendWrite(*v);
    }
    writeToVolume(*v,0, 4096, "blah");

    v->scheduleBackendSync();
    destroyVolume(v,
                  RemoveVolumeCompletely::F);
    v = 0;

    for(unsigned i = num_snapshots; i > 0; --i)
    {
        std::stringstream ss;
        ss << std::setfill('_') << std::setw(2) << (i - 1);
        WriteOnlyVolume* v = 0;
        setVolumeRole(ns, VolumeConfig::WanBackupVolumeRole::WanBackupIncremental);
        v = restartWriteOnlyVolume(vCfg);
        ASSERT_TRUE(v);
        v->restoreSnapshot(SnapshotName(ss.str()));
        waitForThisBackendWrite(*v);
        waitForThisBackendWrite(*v);
        destroyVolume(v,
                      RemoveVolumeCompletely::F);
        setVolumeRole(ns, VolumeConfig::WanBackupVolumeRole::WanBackupNormal);
        restartVolume(vCfg);
        SharedVolumePtr vol = getVolume(vid);
        ASSERT_TRUE(vol != nullptr);
        checkVolume(*vol, 0, 4096, ss.str());
        destroyVolume(vol,
                      DeleteLocalData::T,
                      RemoveVolumeCompletely::F);
        vol = 0;
    }
}

INSTANTIATE_TEST(WriteOnlyVolumeTest);
}

// Local Variables: **
// mode: c++ **
// End: **
