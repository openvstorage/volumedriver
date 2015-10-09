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

#include "DeleteSnapshot.h"
#include "SnapshotPersistor.h"
#include "FilePool.h"
#include "backend/BackendConnectionManager.h"
#include "SnapshotManagement.h"
#include <boost/foreach.hpp>
#include <boost/property_tree/info_parser.hpp>
#include "Types.h"

namespace volumedriver_backup
{
using namespace volumedriver;
namespace be = backend;

DeleteSnapshot::DeleteSnapshot(const bpt::ptree& config_ptree,
                               const std::vector<SnapshotName>& snapshots)
    : configuration_ptree_(config_ptree)
    , target_ptree_(configuration_ptree_.get_child("target_configuration"))
    , snapshots_(snapshots)
    , grace_period_(youtils::GracePeriod(boost::posix_time::seconds(configuration_ptree_.get<uint64_t>("grace_period_in_seconds",
                                                                                                       30))))
{}

std::string
DeleteSnapshot::info()
{
    // Y42 more info
    return "Deletesnapshot task";
}

void
DeleteSnapshot::operator()()
{
    LOG_INFO("Starting the snapshot deletion, requested " << snapshots_.size() <<
             " snapshots deletions");

    std::stringstream ss;
    bpt::write_info(ss, configuration_ptree_);
    LOG_TRACE("Parameters passed : \n" << ss.str());
    BOOST_FOREACH(const auto& snap, snapshots_)
    {
        LOG_TRACE("snapshot " << snap << " deletion requested");
    }

    LOG_INFO(__FUNCTION__);
    FilePool file_pool(configuration_ptree_.get<fs::path>("scratch_dir"));

    be::BackendConnectionManagerPtr
        target_cm(be::BackendConnectionManager::create(target_ptree_,
                                                       RegisterComponent::F));

    const std::string target_namespace(target_ptree_.get<std::string>("namespace"));

    BackendInterfacePtr target_bi(target_cm->newBackendInterface(Namespace(target_namespace)));
    VolumeConfig vc;

    {
        // Check the Volume Config
        fs::path p = file_pool.newFile(VolumeConfig::config_backend_name);
        ALWAYS_CLEANUP_FILE(p);
        target_bi->read(p,
                        VolumeConfig::config_backend_name,
                        InsistOnLatestVersion::T);

        fs::ifstream ifs(p);
        VolumeConfig::iarchive_type ia(ifs);
        ia & vc;
        ifs.close();
        if(vc.wan_backup_volume_role_ ==
           VolumeConfig::WanBackupVolumeRole::WanBackupNormal)
        {
            LOG_ERROR("Not doing deletion operation on a normal volume");
            throw fungi::IOException("No wan backup deletion allowed on normal volume");
        }
    }

    const fs::path snapshots_file_path(file_pool.newFile("snapshots.xml"));
    target_bi->read(snapshots_file_path,
                    snapshotFilename(),
                    InsistOnLatestVersion::T);

    SnapshotPersistor target_snapshot_persistor(snapshots_file_path);
    std::vector<SnapshotNum> snapshot_nums;
    std::vector<SnapshotNum> existing_snapshot_nums;

    target_snapshot_persistor.getAllSnapshots(existing_snapshot_nums);
    if(existing_snapshot_nums.empty())
    {
        LOG_WARN("No snapshots found on backup");
        return;
    }

    BOOST_FOREACH(const auto& snapshot, snapshots_)
    {
        LOG_INFO("trying to get snapshot_num for " << snapshot);
        const SnapshotNum s_num = target_snapshot_persistor.getSnapshotNum(snapshot);
        LOG_INFO("snapshot_num for " << snapshot << " is " << s_num);

        if(s_num == existing_snapshot_nums.back())
        {
            LOG_WARN("Not deleting last snapshot on a backup volume");
            std::stringstream ss;
            ss << "Snapshot " << snapshot << " is the last snapshot and cannot be deleted, exiting with error";
            throw fungi::IOException(ss.str().c_str());


        }
        else if(vc.wan_backup_volume_role_ == VolumeConfig::WanBackupVolumeRole::WanBackupIncremental and
                s_num == existing_snapshot_nums.front())
        {
            LOG_WARN("Not deleting first snapshot on an incremental backup");
            std::stringstream ss;
            ss << "Snapshot " << snapshot << " is the first snapshot and cannot be deleted, exiting with error";
            throw fungi::IOException(ss.str().c_str());
        }
        else
        {
            snapshot_nums.push_back(s_num);
        }
    }

    BOOST_FOREACH(const SnapshotNum s_num, snapshot_nums)
    {
        LOG_INFO("Deleting snapshot number " << s_num);
        target_snapshot_persistor.deleteSnapshot(s_num);
    }

    target_snapshot_persistor.saveToFile(snapshots_file_path, SyncAndRename::T);
    target_bi->write(snapshots_file_path,
                     snapshotFilename(),
                     OverwriteObject::T);
}

}

// Local Variables: **
// mode: c++ **
// End: **
