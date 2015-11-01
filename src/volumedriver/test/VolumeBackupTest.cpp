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

#include "../Backup.h"
#include "../VolManager.h"
#include "../metadata-server/Parameters.h"

#include <boost/filesystem/fstream.hpp>

#include <youtils/System.h>

#include <backend/BackendConnectionManager.h>
#include <backend/Namespace.h>

namespace volumedrivertest
{

using namespace volumedriver;
using namespace volumedriver_backup;

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace yt = youtils;

class VolumeBackupTest
    : public VolManagerTestSetup
{
public:
    VolumeBackupTest()
        : VolManagerTestSetup("VolumeBackupTest")
        , scratch_dir(directory_ / "backup_scratch_dir")
        , backup_backend(directory_ / "backup_local_backend")
    {}

    virtual void
    SetUp()
    {
        VolManagerTestSetup::SetUp();
        fs::create_directories( directory_ / "backup_local_backend");
        fs::create_directories(scratch_dir);
    }

    virtual void
    TearDown()
    {
        VolManagerTestSetup::TearDown();
    }

    void
    create_target_backend_config(bpt::ptree& pt)
    {
        ip::PARAMETER_TYPE(backend_type)(be::BackendType::LOCAL).persist(pt);
        ip::PARAMETER_TYPE(local_connection_path)(backup_backend.string()).persist(pt);
    }

    void
    remove_snapshots_from_backup_namespace(const be::Namespace& backup_namespace)
    {
        bpt::ptree pt;

        ip::PARAMETER_TYPE(backend_type)(be::BackendType::LOCAL).persist(pt);
        ip::PARAMETER_TYPE(local_connection_path)(backup_backend.string()).persist(pt);
        auto cm(be::BackendConnectionManager::create(pt));
        auto bi(cm->newBackendInterface(backup_namespace));

        fs::path snapshots_file_path = FileUtils::create_temp_file_in_temp_dir("snapshots_file");
        ALWAYS_CLEANUP_FILE(snapshots_file_path);

        bi->read(snapshots_file_path,
                 snapshotFilename(),
                 InsistOnLatestVersion::T);
        SnapshotPersistor sp(snapshots_file_path);
        std::vector<SnapshotNum> vec;
        sp.getAllSnapshots(vec);
        for(std::vector<SnapshotNum>::reverse_iterator it = vec.rbegin();
            it != vec.rend();
            ++it)
        {
            sp.deleteSnapshot(*it);
        }
        sp.saveToFile(snapshots_file_path, SyncAndRename::T);
        bi->write(snapshots_file_path,
                  snapshotFilename(),
                  OverwriteObject::T);
    }

    void
    ensure_target_namespace(const be::Namespace& target_ns)
    {
        bpt::ptree pt;
        create_target_backend_config(pt);
        auto cm(be::BackendConnectionManager::create(pt, RegisterComponent::F));
        auto bi(cm->newBackendInterface(target_ns));

        if(not bi->namespaceExists())
        {
            bi->createNamespace();
        }
    }

    void
    get_backup_info(const Namespace& target_ns,
                    bpt::ptree& pt)
    {
        bpt::ptree pt_back;
        create_target_backend_config(pt_back);
        auto cm(be::BackendConnectionManager::create(pt_back, RegisterComponent::F));
        auto bi(cm->newBackendInterface(target_ns));
        fs::path backup_info_path = FileUtils::create_temp_file_in_temp_dir("backup_info.json");
        ALWAYS_CLEANUP_FILE(backup_info_path);
        bi->read(backup_info_path,
                 "BackupInfo.json",
                 InsistOnLatestVersion::T);
        bpt::json_parser::read_json(backup_info_path.string(),
                                    pt);
    }

    void
    check_backup_info(const Namespace& target_ns,
                      uint64_t kept)
    {
        bpt::ptree pt;
        get_backup_info(target_ns,
                        pt);

        ASSERT_EQ(pt.get<uint64_t>("kept"), kept);
        ASSERT_EQ(pt.get<uint64_t>("total_size"),
                  pt.get<uint64_t>("seen"));
        ASSERT_EQ(pt.get<std::string>("status"), "finished");
        ASSERT_EQ(pt.get<uint64_t>("sent_to_backend"), kept);
        ASSERT_EQ(0U, pt.get<uint64_t>("pending"));
    }

    void
    get_backup_snapshots_list(const be::Namespace& target_ns,
                              std::vector<std::string>& result)
    {
        bpt::ptree pt;
        ip::PARAMETER_TYPE(backend_type)(be::BackendType::LOCAL).persist(pt);
        ip::PARAMETER_TYPE(local_connection_path)(backup_backend.string()).persist(pt);
        auto cm(be::BackendConnectionManager::create(pt));
        BackendInterfacePtr bi = cm->newBackendInterface(target_ns);
        fs::path snapshots_path = FileUtils::create_temp_file_in_temp_dir("snapshots.xml");
        bi->read(snapshots_path,
                 "snapshots.xml",
                 InsistOnLatestVersion::T);
        ALWAYS_CLEANUP_FILE(snapshots_path);
        SnapshotPersistor sp(snapshots_path);
        std::vector<SnapshotNum> snaps;
        sp.getAllSnapshots(snaps);

        for (const SnapshotNum num : snaps)
        {
            result.push_back(sp.getSnapshotName(num));
        }
    }

    void
    create_target_config(bpt::ptree& pt,
                         const be::Namespace& target_ns)
    {
        ip::PARAMETER_TYPE(read_cache_serialization_path)(backup_backend.string()).persist(pt);
        pt.put("namespace", target_ns.str());
        const fs::path backup_directory = directory_ / "backup_directory";
        ip::PARAMETER_TYPE(tlog_path)((backup_directory / "tlogs").string()).persist(pt);
        ip::PARAMETER_TYPE(metadata_path)((backup_directory / "metadatastores").string()).persist(pt);
        ip::PARAMETER_TYPE(open_scos_per_volume)(32).persist(pt);
        ip::PARAMETER_TYPE(datastore_throttle_usecs)(4000).persist(pt);
        ip::PARAMETER_TYPE(clean_interval)(60).persist(pt);
        ip::PARAMETER_TYPE(num_threads)(2).persist(pt);
        ip::PARAMETER_TYPE(number_of_scos_in_tlog)(20).persist(pt);

        ip::PARAMETER_TYPE(serialize_read_cache)(0).persist(pt);

        create_target_backend_config(pt);

        MountPointConfigs mp_configs;
        mp_configs.push_back(MountPointConfig((backup_directory / "scocache1").string(), yt::DimensionedValue("1GiB").getBytes()));
        ip::PARAMETER_TYPE(scocache_mount_points)(mp_configs).persist(pt);
        ip::PARAMETER_TYPE(trigger_gap)(yt::DimensionedValue("250MiB")).persist(pt);
        ip::PARAMETER_TYPE(backoff_gap)(yt::DimensionedValue("500MiB")).persist(pt);

        fs::create_directories(backup_directory / "metadatastores");
        fs::create_directories(backup_directory / "tlogs");
        fs::create_directories(backup_directory / "scocache1");
    }

    void
    create_source_config(bpt::ptree& pt,
                         const be::Namespace& source_ns,
                         const std::string& end_snapshot,
                         const std::string& start_snapshot,
                         bool to_backup_backend)
    {
        pt.put("namespace", source_ns.str());
        if(not end_snapshot.empty())
        {
            pt.put("end_snapshot", end_snapshot);
        }
        if(not start_snapshot.empty())
        {
            pt.put("start_snapshot", start_snapshot);
        }
        if(not to_backup_backend)
        {
            VolManager::get()->getBackendConnectionManager()->persist(pt);
        }
        else
        {
            ip::PARAMETER_TYPE(backend_type)(be::BackendType::LOCAL).persist(pt);
            ip::PARAMETER_TYPE(local_connection_path)(backup_backend.string()).persist(pt);
        }
    }

    void
    create_gdbinit_backup()
    {
        fs::ofstream stream(directory_ / ".gdbinit");
        stream << "file /home/immanuel/workspace/code/3.2/VolumeDriver/target/bin/backup_volumedriver" << std::endl;
        stream << "set args --configuration-file=./backup_configuration_file "  << std::endl;
        stream.close();
        sleep(10000);
    }

    void
    create_gdbinit_restore()
    {
        fs::ofstream stream(directory_ / ".gdbinit");
        stream << "file /home/immanuel/workspace/code/3.2/VolumeDriver/target/bin/restore_volumedriver" << std::endl;
        stream << "set args --configuration-file=./restore_configuration_file "  << std::endl;
        stream.close();
        sleep(10000);
    }

    void
    create_backup_config(const be::Namespace& target_ns,
                         const be::Namespace& source_ns,
                         bool to_backup_backend,
                         const std::string& end_snapshot = std::string(""),
                         const std::string& start_snapshot = std::string(""),
                         const std::string& report_threshold = std::string(""))
    {
        bpt::ptree pt;
        bpt::ptree target_ptree;
        create_target_config(target_ptree,
                             target_ns);
        pt.put("global_lock_update_interval_in_seconds", 1);
        pt.put("grace_period_in_seconds", 10);
        pt.put_child("target_configuration", target_ptree);
        int report_interval_in_seconds = yt::System::get_env_with_default("VOLUMEDRIVER_BACKUP_TEST_REPORT_INTERVAL", 1);

        pt.put("report_interval_in_seconds", report_interval_in_seconds);

        bpt::ptree source_ptree;
        create_source_config(source_ptree,
                             source_ns,
                             end_snapshot,
                             start_snapshot,
                             to_backup_backend);
        pt.put_child("source_configuration", source_ptree);
        pt.put("scratch_dir", scratch_dir);
        if(not report_threshold.empty())
        {
            pt.put("report_threshold", report_threshold);
        }

        bpt::json_parser::write_json((directory_ / "backup_configuration_file").string(),
                                     pt);
    }

    Volume*
    restartVolumeFromBackup(const Namespace& ns)
    {
        fungi::ScopedLock l(api::getManagementMutex());
        api::backend_restart(ns,
                             new_owner_tag(),
                             PrefetchVolumeData::T,
                             IgnoreFOCIfUnreachable::T,
                             1024);

        Volume* ret = api::getVolumePointer(ns);
        return ret;
    }

    void
    removeVolumeCompletely(Volume* v)
    {
        VolumeId vid(v->getName());
        {
            fungi::ScopedLock l(api::getManagementMutex());
            api::destroyVolume(vid,
                               DeleteLocalData::T,
                               RemoveVolumeCompletely::F,
                               DeleteVolumeNamespace::F,
                               ForceVolumeDeletion::F);
        }
    }

    void
    create_restore_config(const be::Namespace& target_ns,
                          const boost::optional<be::Namespace>& source_ns,
                          const VolumeId& volume_name = VolumeId(""),
                          const VolumeConfig::WanBackupVolumeRole& role = VolumeConfig::WanBackupVolumeRole::WanBackupNormal)
    {
        bpt::ptree pt;
        bpt::ptree target_ptree;

        VolManager::get()->getBackendConnectionManager()->persist(target_ptree);
        target_ptree.put("namespace", target_ns);
        if(not volume_name.empty())
        {
            target_ptree.put("volume_name", volume_name);
        }

        target_ptree.put("volume_role", role);
        pt.put("global_lock_update_interval_in_seconds", 1);
        pt.put("grace_period_in_seconds", 10);

        pt.put_child("target_configuration", target_ptree);

        if(source_ns)
        {

            bpt::ptree source_ptree;
            ip::PARAMETER_TYPE(backend_type)(be::BackendType::LOCAL).persist(source_ptree);
            ip::PARAMETER_TYPE(local_connection_path)(backup_backend.string()).persist(source_ptree);
            source_ptree.put("namespace", source_ns->str());
            pt.put_child("source_configuration", source_ptree);
        }
        bpt::json_parser::write_json((directory_ / "restore_configuration_file").string(),
                                     pt);
    }

    bool
    start_backup_program()
    {
        fs::remove_all(scratch_dir);
        pid_t pid = fork();

        if(pid == 0)
        {
            const std::string default_loglevel;
            std::string loglevel = yt::System::get_env_with_default("VOLUMEDRIVER_BACKUP_TEST_LOGLEVEL",
                                                                         default_loglevel);
            LOG_NOTIFY("loglevel" << loglevel);

            if(not loglevel.empty())
            {
                loglevel = std::string("--loglevel=") + loglevel;
            }
            else
            {
                loglevel = "--disable-logging";
            }

            int ret = execlp("backup_volumedriver",
                             "backup_volumedriver",
                             ("--configuration-file=" +
                              (directory_ /
                               "backup_configuration_file").string()).c_str(),
                             loglevel.c_str(),
                             (const char*)0);

            if(ret < 0)
            {
                exit(-ret);
            }
        }
        else
        {
            VERIFY(pid > 0);
            int status;
            waitpid(pid,
                    &status,
                    0);
            return status == 0;
        }
        return false;
    }

    bool
    start_snapshot_delete_program(const std::vector<std::string>& snapshots)
    {
        fs::remove_all(scratch_dir);
        pid_t pid = fork();
        if(pid == 0)
        {
            static const size_t alloced = 64;
            VERIFY(snapshots.size() + 3 < alloced);
            const char* args[alloced];
            args[0] =  "backup_volumedriver";
            const std::string arg1("--configuration-file=" + (directory_ / "backup_configuration_file").string()); args[1] = arg1.c_str();
            const std::string default_loglevel;
            std::string loglevel = yt::System::get_env_with_default("VOLUMEDRIVER_BACKUP_TEST_LOGLEVEL",
                                                                         default_loglevel);
            if(not loglevel.empty())
            {
                loglevel = std::string("--loglevel=") + loglevel;
            }
            else
            {
                loglevel = "--disable-logging";
            }

            args[2] = loglevel.c_str();

            const size_t ssize = snapshots.size();
            std::vector<std::string> deleters;

            for(size_t i = 0; i < ssize; ++i)
            {
                deleters.push_back("--delete-snapshot=" + snapshots[i]);
                args[3+i] = deleters[i].c_str();
            }

            args[ssize+3] = 0;


            int ret =  execvp("backup_volumedriver",
                              (char* const*)args);

            if(ret < 0)
            {
                exit(-ret);
            }
        }
        else
        {
            VERIFY(pid > 0);
            int status;
            waitpid(pid,
                    &status,
                    0);
            return status == 0;
        }
        return false;
    }

    bool
    start_restore_program()
    {
        fs::remove_all(scratch_dir);

        std::stringstream jobname;
        pid_t pid = fork();

        if(pid == 0)
        {
            const std::string default_loglevel;
            std::string loglevel = yt::System::get_env_with_default("VOLUMEDRIVER_BACKUP_TEST_LOGLEVEL",
                                                                    default_loglevel);
            if(not loglevel.empty())
            {
                loglevel = std::string("--loglevel=") + loglevel;
            }
            else
            {
                loglevel = "--disable-logging";
            }

            int ret = execlp("restore_volumedriver",
                             "restore_volumedriver",
                             ("--configuration-file=" + (directory_ / "restore_configuration_file").string()).c_str(),
                             loglevel.c_str(),
                             (const char*)0);

            if(ret < 0)
            {
                exit(-ret);
            }
        }
        else
        {
            VERIFY(pid > 0);
            int status;
            waitpid(pid,
                    &status,
                    0);
            return status == 0;
        }
        return false;
    }

    const fs::path scratch_dir;
    const fs::path backup_backend;
};

TEST_P(VolumeBackupTest, test_for_programs)
{
    ASSERT_FALSE(start_backup_program());
}

TEST_P(VolumeBackupTest, simple_backup_restore)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    //    be::Namespace ns1;

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);

    waitForThisBackendWrite(v);

    be::Namespace ns2;

    create_backup_config(ns2,
                         ns1,
                         false,
                         snap1);

    ensure_target_namespace(ns2);


    ASSERT_TRUE(start_backup_program());

    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    std::vector<std::string> snapshots;

    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));

    ASSERT_EQ(1U, snapshots.size());

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns2);

    // create_gdbinit_restore();

    ASSERT_TRUE(start_restore_program());

    Volume* v1 = 0;
    v1 = restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);

    checkVolume(v1,0, 4096, "immanuel");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, report_threshold)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    for(unsigned i = 0; i < 32; ++i)
    {
        writeToVolume(v, 0, 4096,"immanuel");
    }

    const std::string snap1("snap1");

    v->createSnapshot(snap1);

    waitForThisBackendWrite(v);

    Namespace ns2;

    create_backup_config(ns2,
                         ns1,
                         false,
                         snap1,
                         "",
                         "4KiB");

    ensure_target_namespace(ns2);

    //    create_gdbinit_backup();

    ASSERT_TRUE(start_backup_program());

    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    std::vector<std::string> snapshots;

    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));

    ASSERT_EQ(1U, snapshots.size());

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns2);

    // create_gdbinit_restore();

    ASSERT_TRUE(start_restore_program());

    Volume* v1 = 0;
    v1 = restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);

    checkVolume(v1,0, 4096, "immanuel");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, backup_backup)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);

    waitForThisBackendWrite(v);

    Namespace ns2;

    create_backup_config(ns2,
                         ns1,
                         false,
                         snap1);

    ensure_target_namespace(ns2);

    // create_gdbinit_backup();

    ASSERT_TRUE(start_backup_program());

    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    Namespace ns3;

    create_backup_config(ns3,
                         ns2,
                         true);
    ensure_target_namespace(ns3);
    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns3,
                                      4096));

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns3);

    // create_gdbinit_restore();

    ASSERT_TRUE(start_restore_program());

    Volume* v1 = 0;
    v1 = restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);

    checkVolume(v1,0, 4096, "immanuel");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, backup_backup2)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);

    writeToVolume(v, 0, 8192,"bartdv");
    waitForThisBackendWrite(v);

    const std::string snap2("snap2");

    v->createSnapshot(snap2);

    waitForThisBackendWrite(v);

    Namespace ns2;

    create_backup_config(ns2,
                         ns1,
                         false,
                         snap1);

    ensure_target_namespace(ns2);

    // create_gdbinit_backup();

    ASSERT_TRUE(start_backup_program());

    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    create_backup_config(ns2,
                         ns1,
                         false,
                         snap2,
                         snap1);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      8192));

    Namespace ns3;

    create_backup_config(ns3,
                         ns2,
                         true,
                         snap1);
    ensure_target_namespace(ns3);
    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns3,
                                      4096));

    Namespace ns4;

    create_backup_config(ns4,
                         ns2,
                         true,
                         snap2,
                         snap1);
    ensure_target_namespace(ns4);
    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns4,
                                      8192));

    Namespace ns5;

    create_backup_config(ns5,
                         ns4,
                         true);
    ensure_target_namespace(ns5);
    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns5,
                                      8192));


    create_backup_config(ns3,
                         ns5,
                         true);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns3,
                                      8192));

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns3);

    // create_gdbinit_restore();

    ASSERT_TRUE(start_restore_program());

    Volume* v1 = 0;
    v1 = restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);

    checkVolume(v1,0, 4096, "bartdv");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, delete_snapshot)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns);

    writeToVolume(v, 0, 4096,"immanuel");
    const std::string snap1("snap1");
    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);
    writeToVolume(v, 8, 4096,"arne");
    const std::string snap2("snap2");
    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);
    writeToVolume(v, 0, 2*4096,"bart");

    Namespace ns2;
    ensure_target_namespace(ns2);
    create_backup_config(ns2,
                         ns,
                         false,
                         snap1);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    create_backup_config(ns2,
                         ns,
                         false,
                         snap2,
                         snap1);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    std::vector<std::string> snapshots;
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(2U, snapshots.size());

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns2);

    ASSERT_TRUE(start_restore_program());
    Volume* v1 = 0;
    v1=restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);

    checkVolume(v1,0, 4096, "immanuel");
    checkVolume(v1,8, 4096, "arne");
    removeVolumeCompletely(v1);
    restore_to_ptr.reset();

    // Cannot delete the last snapshot
    ASSERT_FALSE(start_snapshot_delete_program(snapshots));

    snapshots.pop_back();

    ASSERT_EQ(1U, snapshots.size());

    ASSERT_TRUE(start_snapshot_delete_program(snapshots));
    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(1U, snapshots.size());
}

TEST_P(VolumeBackupTest, delete_snapshot_has_no_influence_on_restore)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns);

    writeToVolume(v, 0, 4096,"immanuel");
    const std::string snap1("snap1");
    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);
    writeToVolume(v, 8, 4096,"arne");
    const std::string snap2("snap2");
    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);
    writeToVolume(v, 0, 2*4096,"bart");

    Namespace ns2;
    ensure_target_namespace(ns2);
    create_backup_config(ns2,
                         ns,
                         false,
                         snap1);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    create_backup_config(ns2,
                         ns,
                         false,
                         snap2,
                         snap1);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    std::vector<std::string> snapshots;
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(2U, snapshots.size());

    ASSERT_FALSE(start_snapshot_delete_program(snapshots));
    snapshots.pop_back();
    ASSERT_TRUE(start_snapshot_delete_program(snapshots));
    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(1U, snapshots.size());

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns2);

    ASSERT_TRUE(start_restore_program());
    Volume* v1 = 0;
    v1=restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);

    checkVolume(v1,0, 4096, "immanuel");
    checkVolume(v1,8, 4096, "arne");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, start_and_end_snap_are_the_same)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");
    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);

    be::Namespace ns2;
    ensure_target_namespace(ns2);

    create_backup_config(ns2,
                         ns1,
                         false,
                         snap1,
                         snap1);
    //    create_gdbinit_backup();

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      0));

    // Make a base backup to ns3

    Namespace ns3;
    ensure_target_namespace(ns3);
    create_backup_config(ns3,
                         ns1,
                         false);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns3,
                                      4096));

    // Do an incremental backup to ns2
    writeToVolume(v, 0, 4096,"bart");

    const std::string snap2("snap2");

    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);

    create_backup_config(ns2,
                         ns1,
                         false);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    // Apply the incremental to the base

    create_backup_config(ns3,
                         ns2,
                         true);

    ASSERT_TRUE(start_backup_program());

    // restore the backup to the new

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    VolumeId volumeid2("volume2");

    create_restore_config(restore_to,
                          ns3,
                          volumeid2);

    ASSERT_TRUE(start_restore_program());

    Volume* v1 = 0;
    v1 = restartVolumeFromBackup(restore_to);
    checkVolume(v1,0, 4096, "bart");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, end_snapshot_negotiation)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);

    waitForThisBackendWrite(v);

    writeToVolume(v, 0, 4096,"bart");

    const std::string snap2("snap2");

    v->createSnapshot(snap2);

    waitForThisBackendWrite(v);

    be::Namespace ns2;

    create_backup_config(ns2,
                         ns1,
                         false);

    ensure_target_namespace(ns2);
    //    create_gdbinit(JobType::backup);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns2);

    ASSERT_TRUE(start_restore_program());

    Volume* v1 = 0;
    v1 = restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);

    checkVolume(v1,0, 4096, "bart");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, restore_to_backup)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);

    waitForThisBackendWrite(v);
    be::Namespace ns2;
    create_backup_config(ns2,
                         ns1,
                         false);

    ensure_target_namespace(ns2);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns2,
                          VolumeId("volume2"),
                          VolumeConfig::WanBackupVolumeRole::WanBackupBase);

    ASSERT_TRUE(start_restore_program());

    ASSERT_THROW(restartVolumeFromBackup(restore_to),
                 VolManager::VolumeDoesNotHaveCorrectRole);
}

TEST_P(VolumeBackupTest, backup_fails_when_guids_do_not_match)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);

    waitForThisBackendWrite(v);

    be::Namespace ns2;
    create_backup_config(ns2,
                         ns1,
                         false,
                         snap1);
    ensure_target_namespace(ns2);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    v->deleteSnapshot(snap1);
    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);

    writeToVolume(v,0, 4096,"arne");
    const std::string snap2("snap2");
    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);

    create_backup_config(ns2,
                         ns1,
                         false,
                         snap2,
                         snap1);

    ASSERT_FALSE(start_backup_program());
}

TEST_P(VolumeBackupTest, change_name_and_test_no_restore_from_incremental)
{
    VolumeId vid("volume1");
    auto ns1_ptr = make_random_namespace();
    const Namespace& nid = ns1_ptr->ns();

    Volume* v = newVolume(vid,
                          nid);

    VolumeConfig cfg = v->get_config();

    (void)v;
    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);

    waitForThisBackendWrite(v);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    v = 0;

    setVolumeRole(nid, VolumeConfig::WanBackupVolumeRole::WanBackupIncremental);
    ASSERT_THROW(restartVolume(cfg,
                               PrefetchVolumeData::F,
                               CheckVolumeNameForRestart::F),
                 VolManager::VolumeDoesNotHaveCorrectRole);

    create_restore_config(nid,
                          boost::none);
    // Y42 !! just change name ... need to put that in the config
    ASSERT_FALSE(start_restore_program());
    setVolumeRole(nid, VolumeConfig::WanBackupVolumeRole::WanBackupBase);

    create_restore_config(nid,
                          boost::none,
                          VolumeId(""),
                          VolumeConfig::WanBackupVolumeRole::WanBackupIncremental);
    ASSERT_FALSE(start_restore_program());
    create_restore_config(nid,
                          boost::none);

    ASSERT_TRUE(start_restore_program());

    restartVolume(cfg,
                  PrefetchVolumeData::F,
                  CheckVolumeNameForRestart::F);

    v = getVolume(vid);

    EXPECT_TRUE(v);

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    v = 0;
    VolumeId new_vid("new_name_for_volume1");
    create_restore_config(nid,
                          boost::none,
                          new_vid);
    // Y42 !! just change name ... need to put that in the config
    ASSERT_TRUE(start_restore_program());

    restartVolume(cfg,
                  PrefetchVolumeData::F,
                  CheckVolumeNameForRestart::F);

    v = getVolume(vid);

    EXPECT_FALSE(v);


    v = getVolume(new_vid);

    EXPECT_TRUE(v);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeBackupTest, incremental_with_snapshot_negotiation)
{
    auto source_ns_ptr = make_random_namespace();
    const Namespace& source_ns = source_ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          source_ns);

    writeToVolume(v, 0, 4096,"immanuel");

    v->createSnapshot("snap1");
    writeToVolume(v, 0, 4096,"arne");
    waitForThisBackendWrite(v);

    Namespace backup_ns;
    create_backup_config(backup_ns,
                         source_ns,
                         false);

    ensure_target_namespace(backup_ns);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(backup_ns,
                                      4096));

    {
        auto restore_ns_ptr = make_random_namespace();
        const be::Namespace& restore_ns = restore_ns_ptr->ns();

        create_restore_config(restore_ns,
                              backup_ns);

        ASSERT_TRUE(start_restore_program());

        Volume* v1 = restartVolumeFromBackup(restore_ns);
        ASSERT_TRUE(v1);
        checkVolume(v1,0, 4096, "immanuel");
        removeVolumeCompletely(v1);
    }

    v->createSnapshot("snap2");
    writeToVolume(v, 0, 4096,"immanuel");
    waitForThisBackendWrite(v);

    create_backup_config(backup_ns,
                         source_ns,
                         false);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(backup_ns,
                                      4096));

    {
        auto restore_ns_ptr = make_random_namespace();
        const be::Namespace& restore_ns = restore_ns_ptr->ns();

        create_restore_config(restore_ns,
                              backup_ns);

        ASSERT_TRUE(start_restore_program());

        Volume* v1 = restartVolumeFromBackup(restore_ns);
        ASSERT_TRUE(v1);
        checkVolume(v1,0, 4096, "arne");
        removeVolumeCompletely(v1);
    }
}

TEST_P(VolumeBackupTest, incremental_with_faulty_snapshot)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);


    (void)v;
    writeToVolume(v, 0, 4096,"immanuel");

    v->createSnapshot("snap1");

    waitForThisBackendWrite(v);

    Namespace ns2;
    create_backup_config(ns2,
                         ns1,
                         false);

    ensure_target_namespace(ns2);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns2);

    ASSERT_TRUE(start_restore_program());

    Volume* v1 = restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);
    checkVolume(v1,0, 4096, "immanuel");
    removeVolumeCompletely(v1);
    v1 = 0;

    v->deleteSnapshot("snap1");
    v->createSnapshot("snap1");
    waitForThisBackendWrite(v);

    writeToVolume(v, 0, 4096,"arne");
    v->createSnapshot("snap2");
    writeToVolume(v, 0, 4096,"immanuel");
    waitForThisBackendWrite(v);

    restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to_2 = restore_to_ptr->ns();

    create_backup_config(be::Namespace(),
                         ns1,
                         false);

    ASSERT_FALSE(start_backup_program());

    create_restore_config(restore_to_2,
                          ns2);

    ASSERT_TRUE(start_restore_program());

    v1 = restartVolumeFromBackup(restore_to_2);
    ASSERT_TRUE(v1);
    checkVolume(v1,0, 4096, "immanuel");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, string_of_incremental_backup_restore)
{
    auto source_ns_ptr(make_random_namespace());
    const Namespace& source_ns = source_ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          source_ns);

    writeToVolume(v, 0, 4096, "immanuel");

    const std::string snap1("snap1");
    v->createSnapshot(snap1);

    waitForThisBackendWrite(v);

    writeToVolume(v,0, 4096, "arne");

    Namespace backup_ns;
    create_backup_config(backup_ns,
                         source_ns,
                         false,
                         snap1);

    ensure_target_namespace(backup_ns);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(backup_ns,
                                      4096));

    {
        auto restore_ns_ptr(make_random_namespace());
        const be::Namespace& restore_ns = restore_ns_ptr->ns();

        create_restore_config(restore_ns,
                              backup_ns);

        ASSERT_TRUE(start_restore_program());

        Volume* v1 = restartVolumeFromBackup(restore_ns);
        ASSERT_TRUE(v1);

        checkVolume(v1, 0, 4096, "immanuel");
        removeVolumeCompletely(v1);
    }

    writeToVolume(v, 0, 4096, "arne");
    writeToVolume(v, 8, 4096, "bart");

    const std::string snap2("snap2");
    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);

    create_backup_config(backup_ns,
                         source_ns,
                         false,
                         snap2,
                         snap1);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(backup_ns,
                                      8192));

    {
        auto restore_ns_ptr(make_random_namespace());
        const be::Namespace& restore_ns = restore_ns_ptr->ns();

        create_restore_config(restore_ns,
                              backup_ns);

        ASSERT_TRUE(start_restore_program());

        Volume* v1 = restartVolumeFromBackup(restore_ns);
        ASSERT_TRUE(v1);
        checkVolume(v1, 0, 4096, "arne");
        checkVolume(v1, 8, 4096, "bart");
        removeVolumeCompletely(v1);
    }
}

TEST_P(VolumeBackupTest, simple_incremental_and_apply)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");
    const std::string snap1("snap1");
    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);
    writeToVolume(v, 8, 4096,"arne");
    const std::string snap2("snap2");
    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);
    writeToVolume(v, 0, 2*4096,"bart");
    Namespace ns2;
    create_backup_config(ns2,
                         ns1,
                         false,
                         snap2,
                         snap1);
    ensure_target_namespace(ns2);
    //    create_gdbinit_backup();

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    Namespace ns3;
    create_backup_config(ns3,
                         ns1,
                         false,
                         snap1);
    ensure_target_namespace(ns3);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns3,
                                      4096));

    create_backup_config(ns3,
                         ns2,
                         true,
                         snap2,
                         snap1);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns3,
                                      4096));

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns3);

    ASSERT_TRUE(start_restore_program());
    Volume* v1 = 0;
    v1=restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);

    checkVolume(v1,0, 4096, "immanuel");
    checkVolume(v1,8, 4096, "arne");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, simple_backup_restore_with_clones)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);
    auto ns2_ptr = make_random_namespace();
    const Namespace& clone_ns1 = ns2_ptr->ns();

    Volume* c1 = 0;
    c1 = createClone("clone1",
                     clone_ns1,
                     ns1,
                     snap1);

    writeToVolume(c1, 8, 4096, "arne");
    c1->createSnapshot(snap1);
    waitForThisBackendWrite(c1);
    auto ns3_ptr = make_random_namespace();
    const Namespace& clone_ns2 = ns3_ptr->ns();

    Volume* c2 = 0;
    c2 = createClone("clone2",
                     clone_ns2,
                     clone_ns1,
                     snap1);

    writeToVolume(c2, 16, 4096,"bart");
    c2->createSnapshot(snap1);
    waitForThisBackendWrite(c2);

    Namespace ns2;
    create_backup_config(ns2,
                         clone_ns2,
                         false,
                         snap1);

    ensure_target_namespace(ns2);

    //    create_gdbinit(JobType::backup);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      12288));

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns2);

    ASSERT_TRUE(start_restore_program());

    Volume* v1 = restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);

    checkVolume(v1,0, 4096, "immanuel");
    checkVolume(v1,8, 4096, "arne");
    checkVolume(v1,16, 4096, "bart");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, impotence_test)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    VolumeId vid1("volume1");

    Volume* v = newVolume(vid1,
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);

    be::Namespace ns2;

    create_backup_config(ns2,
                         ns1,
                         false);

    ensure_target_namespace(ns2);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      0));

    writeToVolume(v, 0, 4096,"bart");

    const std::string snap2("snap2");

    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);

    create_backup_config(ns2,
                         ns1,
                         false);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      0));

    auto restore_to_ptr = make_random_namespace();

    const be::Namespace& restore_to = restore_to_ptr->ns();

    create_restore_config(restore_to,
                          ns2);

    ASSERT_TRUE(start_restore_program());

    Volume* v1 = 0;
    v1 = restartVolumeFromBackup(restore_to);
    ASSERT_TRUE(v1);

    checkVolume(v1,0, 4096, "bart");
    removeVolumeCompletely(v1);
}

TEST_P(VolumeBackupTest, unfuck_fucked_up_backend)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    VolumeId vid1("volume1");

    Volume* v = newVolume(vid1,
                          ns1);

    writeToVolume(v, 0, 4096,"immanuel");

    const std::string snap1("snap1");

    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);

    Namespace ns2;

    create_backup_config(ns2,
                         ns1,
                         false);

    ensure_target_namespace(ns2);

    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));

    remove_snapshots_from_backup_namespace(ns2);
    ASSERT_TRUE(start_backup_program());
    ASSERT_NO_THROW(check_backup_info(ns2,
                                      4096));
}

TEST_P(VolumeBackupTest, dont_delete_last_or_first_snapshot)
{
    auto ns_ptr = make_random_namespace();
    const Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume(VolumeId("volume1"),
                          ns);

    const std::string snap1("snap1");
    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);

    const std::string snap2("snap2");
    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);

    const std::string snap3("snap3");
    v->createSnapshot(snap3);
    waitForThisBackendWrite(v);

    const std::string snap4("snap4");
    v->createSnapshot(snap4);
    waitForThisBackendWrite(v);

    std::vector<std::string> snapshots;
    Namespace ns1;
    ensure_target_namespace(ns1);
    create_backup_config(ns1,
                         ns,
                         false,
                         snap1);

    ASSERT_TRUE(start_backup_program());

    create_backup_config(ns1,
                         ns,
                         false,
                         snap2);
    ASSERT_TRUE(start_backup_program());

    ASSERT_NO_THROW(get_backup_snapshots_list(ns1,
                                              snapshots));
    ASSERT_EQ(2U, snapshots.size());

    std::vector<std::string> snapshots_to_delete;

    snapshots_to_delete.push_back(snapshots.back());

    ASSERT_FALSE(start_snapshot_delete_program(snapshots_to_delete));

    snapshots.clear();
    EXPECT_NO_THROW(get_backup_snapshots_list(ns1,
                                              snapshots));
    ASSERT_EQ(2U, snapshots.size());

    snapshots_to_delete.clear();
    snapshots_to_delete.push_back(snapshots.front());

    ASSERT_TRUE(start_snapshot_delete_program(snapshots_to_delete));

    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns1,
                                              snapshots));
    ASSERT_EQ(1U, snapshots.size());

    snapshots_to_delete.clear();
    snapshots_to_delete.push_back(snapshots.back());

    ASSERT_FALSE(start_snapshot_delete_program(snapshots_to_delete));

    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns1,
                                              snapshots));
    ASSERT_EQ(1U, snapshots.size());

    Namespace ns2;
    ensure_target_namespace(ns2);
    create_backup_config(ns2,
                         ns,
                         false,
                         snap2,
                         snap1);
    ASSERT_TRUE(start_backup_program());

    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(2U, snapshots.size());

    create_backup_config(ns2,
                         ns,
                         false,
                         snap3);
    ASSERT_TRUE(start_backup_program());

    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(3U, snapshots.size());

    create_backup_config(ns2,
                         ns,
                         false,
                         snap4);
    ASSERT_TRUE(start_backup_program());

    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(4U, snapshots.size());

    snapshots_to_delete.clear();
    snapshots_to_delete.push_back(snapshots.front());

    ASSERT_FALSE(start_snapshot_delete_program(snapshots_to_delete));

    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(4U, snapshots.size());

    snapshots_to_delete.clear();

    snapshots_to_delete.push_back(snapshots.back());
    ASSERT_FALSE(start_snapshot_delete_program(snapshots_to_delete));
    snapshots.clear();

    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(4U, snapshots.size());

    snapshots_to_delete.clear();
    snapshots_to_delete.push_back(snapshots[0]);
    snapshots_to_delete.push_back(snapshots[1]);

    ASSERT_FALSE(start_snapshot_delete_program(snapshots_to_delete));

    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(4U, snapshots.size());

    snapshots_to_delete.clear();
    snapshots_to_delete.push_back(snapshots[2]);
    snapshots_to_delete.push_back(snapshots[3]);

    ASSERT_FALSE(start_snapshot_delete_program(snapshots_to_delete));

    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(4U, snapshots.size());

    snapshots_to_delete.clear();

    snapshots_to_delete.push_back(snapshots[1]);
    snapshots_to_delete.push_back(snapshots[2]);

    ASSERT_TRUE(start_snapshot_delete_program(snapshots_to_delete));

    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(2U, snapshots.size());

    ASSERT_FALSE(start_snapshot_delete_program(snapshots));
    snapshots.clear();
    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(2U, snapshots.size());

    snapshots_to_delete.clear();

    snapshots_to_delete.push_back(snapshots.front());
    ASSERT_FALSE(start_snapshot_delete_program(snapshots_to_delete));
    snapshots.clear();

    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(2U, snapshots.size());

    snapshots_to_delete.clear();

    snapshots_to_delete.push_back(snapshots.back());
    ASSERT_FALSE(start_snapshot_delete_program(snapshots_to_delete));
    snapshots.clear();

    ASSERT_NO_THROW(get_backup_snapshots_list(ns2,
                                              snapshots));
    ASSERT_EQ(2U, snapshots.size());
}

INSTANTIATE_TEST(VolumeBackupTest);

}

// Local Variables: **
// mode: c++ **
// End: **
