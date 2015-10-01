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

#include "VolManagerTestSetup.h"

#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "../Restore.h"
#include "../Api.h"
#include "../WriteOnlyVolume.h"

namespace volumedrivertest
{

using namespace volumedriver;
namespace bpt = boost::property_tree;
namespace bu = volumedriver_backup;
namespace fs = boost::filesystem;

typedef VolumeConfig::WanBackupVolumeRole BackupRole;
typedef boost::optional<BackupRole> MaybeBackupRole;
typedef boost::optional<std::string> MaybeString;

class SimpleBackupRestoreTest
    : public VolManagerTestSetup
{
public:
    SimpleBackupRestoreTest()
        : VolManagerTestSetup("SimpleBackupRestoreTest")
        , source_dir_(directory_ / "source")
        , target_dir_(directory_ / "target")
    {}

    virtual ~SimpleBackupRestoreTest()
    {}

    virtual void
    SetUp()
    {
        VolManagerTestSetup::SetUp();

        fs::remove_all(source_dir_);
        fs::create_directories(source_dir_);
        fs::remove_all(target_dir_);
        fs::create_directories(target_dir_);
    }

    virtual void
    TearDown()
    {
        fs::remove_all(target_dir_);
        fs::remove_all(source_dir_);

        VolManagerTestSetup::TearDown();
    }

    const fs::path
    make_config_file(const backend::Namespace& nspace,
                     const MaybeString& maybe_src,
                     const MaybeString& maybe_new_name,
                     const MaybeBackupRole& maybe_new_role)
    {
        bpt::ptree dpt;
        cm_->persist(dpt, ReportDefault::T);
        dpt.put("namespace", nspace);

        if (maybe_new_role)
        {
            std::stringstream ss;
            ss << *maybe_new_role;
            dpt.put("volume_role", ss.str());
        }

        if (maybe_new_name)
        {
            dpt.put("volume_name", *maybe_new_name);
        }

        bpt::ptree pt;
        pt.put_child("target_configuration", dpt);

        if (maybe_src)
        {
            bpt::ptree spt;
            cm_->persist(spt, ReportDefault::T);
            spt.put("namespace", *maybe_src);
            spt.put("volume_name", *maybe_src);

            pt.put_child("source_configuration", spt);
        }

        const fs::path p(target_dir_ / "backup_restore.cfg");
        bpt::json_parser::write_json(p.string(), pt);
        return p;
    }

    void
    test_transition(BackupRole from,
                    BackupRole to,
                    bool allowed)
    {
        auto ns_ptr = make_random_namespace();

        const backend::Namespace& nspace = ns_ptr->ns();

        //        const backend::Namespace nspace;
        const VolumeId vid("backup");

        auto wov = newWriteOnlyVolume(vid,
                                      nspace,
                                      BackupRole::WanBackupBase);
        ASSERT_TRUE(wov);

        const auto cfg(wov->get_config());

        const std::string pattern("back up?");
        const size_t size = 1024 * 1024;
        writeToVolume(wov, 0, size, pattern);
        wov->createSnapshot("snap");
        waitForThisBackendWrite(wov);
        destroyVolume(wov,
                      RemoveVolumeCompletely::F);

        setVolumeRole(nspace, from);

        const fs::path cfg_file(make_config_file(nspace,
                                                 MaybeString(),
                                                 MaybeString(),
                                                 MaybeBackupRole(to)));

        if (allowed)
        {
            EXPECT_NO_THROW(bu::Restore(cfg_file,
                                        false,
                                        youtils::GracePeriod(boost::posix_time::seconds(2)))());
            if (to == BackupRole::WanBackupNormal)
            {
                {
                    fungi::ScopedLock l(api::getManagementMutex());
                    api::backend_restart(nspace,
                                         new_owner_tag(),
                                         PrefetchVolumeData::T,
                                         IgnoreFOCIfUnreachable::T,
                                         1024);
                }
                Volume* v = getVolume(vid);
                checkVolume(v, 0, size, pattern);
            }
        }
        else
        {
            bu::Restore r(cfg_file,
                          false,
                          youtils::GracePeriod(boost::posix_time::seconds(2)));
            EXPECT_THROW(r(), bu::RestoreException);
        }
    }

protected:
    const fs::path source_dir_;
    const fs::path target_dir_;
};

TEST_P(SimpleBackupRestoreTest, no_promotion_without_snapshot)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    //const Namespace nspace;
    const VolumeId vid("backup");

    auto wov = newWriteOnlyVolume(vid,
                                  nspace,
                                  BackupRole::WanBackupBase);
    ASSERT_TRUE(wov);

    const auto cfg(wov->get_config());

    const std::string pattern("back up?");
    const size_t size = 1024 * 1024;
    writeToVolume(wov, 0, size, pattern);
    wov->scheduleBackendSync();
    waitForThisBackendWrite(wov);
    destroyVolume(wov,
                  RemoveVolumeCompletely::F);

    const fs::path
        cfg_file(make_config_file(nspace,
                                  MaybeString(),
                                  MaybeString(),
                                  MaybeBackupRole(BackupRole::WanBackupNormal)));

    {
        bu::Restore r(cfg_file,
                      false,
                      youtils::GracePeriod(boost::posix_time::seconds(2)));
        EXPECT_THROW(r(), bu::RestoreException);
    }

    {
        wov = restartWriteOnlyVolume(cfg);
        wov->createSnapshot("snap");
        waitForThisBackendWrite(wov);
        destroyVolume(wov,
                      RemoveVolumeCompletely::F);
    }

    bu::Restore(cfg_file,
                false,
                youtils::GracePeriod(boost::posix_time::seconds(2)))();
    {
        fungi::ScopedLock l(api::getManagementMutex());
        api::backend_restart(nspace,
                             new_owner_tag(),
                             PrefetchVolumeData::T,
                             IgnoreFOCIfUnreachable::T, 1024);
    }

    Volume* v = getVolume(vid);
    checkVolume(v, 0, size, pattern);
}

TEST_P(SimpleBackupRestoreTest, discard_trailing_tlogs_on_promotion)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    //const Namespace nspace;
    const VolumeId vid("backup");

    auto wov = newWriteOnlyVolume(vid,
                                  nspace,
                                  BackupRole::WanBackupBase);
    ASSERT_TRUE(wov);

    const auto cfg(wov->get_config());

    const std::string pattern("back up?");
    const size_t size = 1024 * 1024;
    writeToVolume(wov, 0, size, pattern);
    wov->createSnapshot("snap");

    const std::string pattern2("This is invisible. Or is it?");
    writeToVolume(wov, 0, size, pattern2);
    wov->scheduleBackendSync();
    waitForThisBackendWrite(wov);
    destroyVolume(wov,
                  RemoveVolumeCompletely::F);

    const fs::path
        cfg_file(make_config_file(nspace,
                                  MaybeString(),
                                  MaybeString(),
                                  MaybeBackupRole(BackupRole::WanBackupNormal)));

    bu::Restore(cfg_file,
                false,
                youtils::GracePeriod(boost::posix_time::seconds(2)))();
    {
        fungi::ScopedLock l(api::getManagementMutex());
        api::backend_restart(nspace,
                             new_owner_tag(),
                             PrefetchVolumeData::T,
                             IgnoreFOCIfUnreachable::T,
                             1024);
    }

    Volume* v = getVolume(vid);
    checkVolume(v, 0, size, pattern);
}

TEST_P(SimpleBackupRestoreTest, rollback_to_previous_snap_if_snapshot_didnt_make_it)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    // const Namespace nspace;
    const VolumeId vid("backup");


    auto wov = newWriteOnlyVolume(vid,
                                  nspace,
                                  BackupRole::WanBackupBase);
    ASSERT_TRUE(wov);

    const auto cfg(wov->get_config());

    const std::string pattern("a mysteriously returning message");
    const size_t size = 1024 * 1024;
    writeToVolume(wov, 0, size, pattern);
    const std::string snap("snap");
    wov->createSnapshot(snap);
    waitForThisBackendWrite(wov);
    const std::string pattern2("a mysteriously disappearing message");
    writeToVolume(wov, 0, size, pattern2);
    wov->scheduleBackendSync();
    waitForThisBackendWrite(wov);
    {
        SCOPED_DESTROY_WRITE_ONLY_VOLUME_UNBLOCK_BACKEND_FOR_BACKEND_RESTART(wov, 3);
        wov->createSnapshot("snap2");

        const SnapshotPersistor& sp =
            wov->getSnapshotManagement().getSnapshotPersistor();
        SnapshotManagement::writeSnapshotPersistor(sp,
                                                   cm_->newBackendInterface(nspace));
    }

    const fs::path
        cfg_file(make_config_file(nspace,
                                  MaybeString(),
                                  MaybeString(),
                                  MaybeBackupRole(BackupRole::WanBackupNormal)));

    bu::Restore(cfg_file,
                false,
                youtils::GracePeriod(boost::posix_time::seconds(2)))();
    {
        fungi::ScopedLock l(api::getManagementMutex());
        api::backend_restart(nspace,
                             new_owner_tag(),
                             PrefetchVolumeData::T,
                             IgnoreFOCIfUnreachable::T,
                             1024);
    }

    Volume* v = getVolume(vid);
    checkVolume(v, 0, size, pattern);
    std::list<std::string> snaps;
    v->listSnapshots(snaps);
    EXPECT_EQ(1U, snaps.size());
    EXPECT_EQ(snap, snaps.front());
}

TEST_P(SimpleBackupRestoreTest,
       no_promotion_if_the_last_snap_is_not_on_backend_and_there_are_trailing_tlogs)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    // const Namespace nspace;
    const VolumeId vid("backup");

    auto wov = newWriteOnlyVolume(vid,
                                  nspace,
                                  BackupRole::WanBackupBase);
    ASSERT_TRUE(wov);

    const auto cfg(wov->get_config());

    const std::string pattern("blah");
    const size_t size = 1024 * 1024;
    writeToVolume(wov, 0, size, pattern);
    const std::string snap("snap");
    wov->createSnapshot(snap);
    waitForThisBackendWrite(wov);
    const std::string pattern2("meh");
    writeToVolume(wov, 0, size, pattern2);
    wov->scheduleBackendSync();
    waitForThisBackendWrite(wov);

    {
        SCOPED_DESTROY_WRITE_ONLY_VOLUME_UNBLOCK_BACKEND_FOR_BACKEND_RESTART(wov, 3);
        wov->createSnapshot("snap2");
        const std::string pattern3("blub");
        writeToVolume(wov, 0, size, pattern3);
        wov->scheduleBackendSync();

        const SnapshotPersistor& sp =
            wov->getSnapshotManagement().getSnapshotPersistor();
        SnapshotManagement::writeSnapshotPersistor(sp,
                                                   cm_->newBackendInterface(nspace));
    }

    const fs::path
        cfg_file(make_config_file(nspace,
                                  MaybeString(),
                                  MaybeString(),
                                  MaybeBackupRole(BackupRole::WanBackupNormal)));

    bu::Restore r(cfg_file,
                  false,
                  youtils::GracePeriod(boost::posix_time::seconds(2)));
    EXPECT_THROW(r(), bu::RestoreException);
}

TEST_P(SimpleBackupRestoreTest, promotion_from_base_to_normal)
{
    test_transition(BackupRole::WanBackupBase,
                    BackupRole::WanBackupNormal,
                    true);
}

TEST_P(SimpleBackupRestoreTest, promotion_from_base_to_base)
{
    test_transition(BackupRole::WanBackupBase,
                    BackupRole::WanBackupBase,
                    true);
}

TEST_P(SimpleBackupRestoreTest, no_promotion_from_base_to_incremental)
{
    test_transition(BackupRole::WanBackupBase,
                    BackupRole::WanBackupIncremental,
                    false);
}

TEST_P(SimpleBackupRestoreTest, promotion_from_normal_to_normal)
{
    test_transition(BackupRole::WanBackupNormal,
                    BackupRole::WanBackupNormal,
                    true);
}

TEST_P(SimpleBackupRestoreTest, no_promotion_from_normal_to_base)
{
    test_transition(BackupRole::WanBackupNormal,
                    BackupRole::WanBackupBase,
                    false);
}

TEST_P(SimpleBackupRestoreTest, no_promotion_from_normal_to_incremental)
{
    test_transition(BackupRole::WanBackupNormal,
                    BackupRole::WanBackupIncremental,
                    false);
}

TEST_P(SimpleBackupRestoreTest, no_promotion_from_incremental_to_normal)
{
    test_transition(BackupRole::WanBackupIncremental,
                    BackupRole::WanBackupNormal,
                    false);
}

TEST_P(SimpleBackupRestoreTest, no_promotion_from_incremental_to_base)
{
    test_transition(BackupRole::WanBackupIncremental,
                    BackupRole::WanBackupBase,
                    false);
}

TEST_P(SimpleBackupRestoreTest, promotion_from_incremental_to_incremental)
{
    test_transition(BackupRole::WanBackupIncremental,
                    BackupRole::WanBackupIncremental,
                    true);
}

INSTANTIATE_TEST(SimpleBackupRestoreTest);

}

// Local Variables: **
// mode: c++ **
// End: **
