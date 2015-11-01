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

#include "Backup.h"

#include <boost/dynamic_bitset.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/scope_exit.hpp>

#include "failovercache/fungilib/Mutex.h"

#include <backend/BackendConnectionManager.h>

#include <volumedriver/Api.h>
#include <volumedriver/BackwardTLogReader.h>
#include <volumedriver/BackendNamesFilter.h>
#include <volumedriver/BackendTasks.h>
#include <volumedriver/SnapshotManagement.h>
#include <volumedriver/TLogReaderUtils.h>
#include <volumedriver/TransientException.h>
#include <volumedriver/Types.h>
#include <volumedriver/VolManager.h>
#include <volumedriver/VolumeConfig.h>
#include <volumedriver/VolumeThreadPool.h>
#include <volumedriver/WriteOnlyVolume.h>
#include <youtils/Catchers.h>

namespace volumedriver_backup
{

namespace yt = youtils;
using namespace volumedriver;

class RetryCreateVolume
{};

const uint64_t
Backup::report_times_default_ = 120;

Backup::Backup(const boost::property_tree::ptree& config_ptree)
     : configuration_ptree_(config_ptree)
     , source_ptree(configuration_ptree_.get_child("source_configuration"))
     , target_ptree(configuration_ptree_.get_child("target_configuration"))
     , target_namespace(target_ptree.get<std::string>("namespace"))
     , end_snapshot_number(0)
     , report_threshold(yt::DimensionedValue(configuration_ptree_.get<std::string>("report_threshold", "160MiB")).getBytes())
     , status_(boost::posix_time::seconds(configuration_ptree_.get<uint64_t>("report_interval_in_seconds",
                                                                             report_times_default_)),
               target_namespace)

     , grace_period_(youtils::GracePeriod(boost::posix_time::seconds(configuration_ptree_.get<uint64_t>("grace_period_in_seconds",
                                                                                                        30))))
     , total_size_(0)
{
    LOG_INFO("Report threshold is " << report_threshold);
    LOG_INFO("grace_period_in_seconds is " << static_cast<boost::posix_time::time_duration>(grace_period_));

    LOG_INFO("Reporting every " << status_.report_interval() << " seconds");
}

void
Backup::print_configuration_documentation(std::ostream& os)
{
    LOG_INFO(__FUNCTION__);
    os << "Back up a source volume to a target volume";
}

void
Backup::setup_file_pool()
{
    LOG_INFO(__FUNCTION__);
    LOG_INFO("Checking scratch dir");
    const fs::path scratch_dir = configuration_ptree_.get<fs::path>("scratch_dir");
    LOG_INFO("scratch_dir is " << scratch_dir);
    file_pool.reset(new FilePool(scratch_dir));
}

void
Backup::get_source_snapshots_file()
{
    LOG_INFO(__FUNCTION__);
    VERIFY(file_pool);

    LOG_INFO("Setting up connection Manager");
    source_connection_manager =
        backend::BackendConnectionManager::create(source_ptree, RegisterComponent::F);

    const std::string source_namespace = source_ptree.get<std::string>("namespace");

    LOG_INFO("Reading the snapshots file from the source " << source_namespace);

    BackendInterfacePtr source_bi = source_connection_manager->newBackendInterface(backend::Namespace(source_namespace));

    const fs::path snapshots_file_path = file_pool->newFile("snapshots.xml");

    source_bi->read(snapshots_file_path,
                    snapshotFilename(),
                    InsistOnLatestVersion::T);

    source_snapshot_persistor.reset(new SnapshotPersistor(snapshots_file_path));

    if(not source_snapshot_persistor->hasUUIDSpecified())
    {
        LOG_FATAL("Could not find GUIDS in source " << source_namespace << " snapshots file " << snapshots_file_path);
        throw BackupException("No GUID's in the snapshots file, update volumedriver first");
    }
    nsid.set(0,
             std::move(source_bi));
}

void
Backup::figure_out_end_snapshot()
{
    LOG_INFO(__FUNCTION__);
    VERIFY(source_snapshot_persistor);
    VERIFY(not start_snapshot_);

    const boost::optional<std::string>
        optional_end_snapshot(source_ptree.get_optional<std::string>("end_snapshot"));

    if(optional_end_snapshot)
    {
        if(not source_snapshot_persistor->snapshotExists(*optional_end_snapshot))
        {

            LOG_FATAL("Could not find end snapshot " << *optional_end_snapshot <<
                      " in source");
            throw BackupException("Could not find snapshot");
        }
        end_snapshot_name = *optional_end_snapshot;
        end_snapshot_number = source_snapshot_persistor->getSnapshotNum(end_snapshot_name);
    }
    else
    {
        std::vector<SnapshotNum> snapshots;
        source_snapshot_persistor->getAllSnapshots(snapshots);
        if(snapshots.empty())
        {
            LOG_FATAL("Cannot backup a volume that doesn't have snapshots");
            throw BackupException("No snapshots on source");

        }
        end_snapshot_number = snapshots.back();
        end_snapshot_name = source_snapshot_persistor->getSnapshotName(end_snapshot_number);
    }
    VERIFY(not end_snapshot_name.empty());
    LOG_INFO("end snapshot is " << end_snapshot_name);
    status_.end_snapshot(end_snapshot_name);
}

void
Backup::figure_out_start_snapshot()
{
    LOG_INFO(__FUNCTION__);
    VERIFY(source_snapshot_persistor);

    start_snapshot_ = source_ptree.get_optional<std::string>("start_snapshot");

    if(start_snapshot_ and
       not source_snapshot_persistor->snapshotExists(*start_snapshot_))
    {
        LOG_FATAL("Could not find start snapshot " << *start_snapshot_ << " in source");
        throw BackupException("Could not find snapshot");
    }
}

void
Backup::get_source_volume_info()
{
    LOG_INFO(__FUNCTION__);
    source_volume_config.reset(new VolumeConfig());

    LOG_INFO("Getting the source volume config from the backend");
    try
    {
        const fs::path p = file_pool->newFile("source_volume_config");
        nsid.get(0)->read(p,
                          VolumeConfig::config_backend_name,
                          InsistOnLatestVersion::T);
        fs::ifstream ifs(p);
        VolumeConfig::iarchive_type ia(ifs);
        ia & *source_volume_config;
        ifs.close();
    }
    catch(std::exception& e)
    {
        LOG_FATAL("Could not get source volume info from it's backend, " << e.what());
        throw;
    }
    catch(...)
    {
        LOG_FATAL("Could not get source volume info from it's backend, unknown exception" );
        throw;
    }
}

void
Backup::get_target_volume_info()
{
    LOG_INFO(__FUNCTION__);
    try
    {
        target_volume_config.reset(new VolumeConfig());
        const fs::path p = file_pool->newFile("target_volume_config");
        BackendInterfacePtr bip = VolManager::get()->createBackendInterface(backend::Namespace(target_namespace));

        bip->read(p,
                  VolumeConfig::config_backend_name,
                  InsistOnLatestVersion::T);
        fs::ifstream ifs(p);
        VolumeConfig::iarchive_type ia(ifs);
        ia & *target_volume_config;
        ifs.close();
    }
    catch(std::exception& e)
    {
        LOG_FATAL("Could not get source volume info from it's backend, " << e.what());
        throw;
    }
    catch(...)
    {
        LOG_FATAL("Could not get source volume info from it's backend, unknown exception" );
        throw;
    }
}

bool
Backup::preexisting_volume_checks()
{
    LOG_INFO(__FUNCTION__);
    VERIFY(source_snapshot_persistor);
    VERIFY(target_volume_);

    api::getManagementMutex().assertLocked();

    if(start_snapshot_)
    {
        const std::string& start_snapshot_name = *start_snapshot_;
        const SnapshotNum start_snapshot_num = source_snapshot_persistor->getSnapshotNum(*start_snapshot_);
        const UUID start_snapshot_uuid = source_snapshot_persistor->getUUID(start_snapshot_num);

        LOG_INFO("Backup target volume existed, figuring out if we need to do work");


        // We are going to apply an incremental with snapshot matching
        //const SnapshotNum start_snapshot_num = source_snapshot_persistor.getSnapshotNum(*start_snapshot);
        if(not api::checkSnapshotUUID(target_volume_.get(),
                                      start_snapshot_name,
                                      start_snapshot_uuid))
        {
            LOG_FATAL("Snapshot with name " << start_snapshot_name << " has non matching guids, exiting");
            throw BackupException("Non matching guids");
        }
        else if(api::snapshotExists(target_volume_.get(),
                                    end_snapshot_name))
        {
            const UUID end_snapshot_uuid = source_snapshot_persistor->getUUID(end_snapshot_number);
            if(api::checkSnapshotUUID(target_volume_.get(),
                                      end_snapshot_name,
                                      end_snapshot_uuid))
            {
                LOG_INFO("Start and end are already on target and guids match, lovely!");
                LOG_INFO("Exiting early");
                {
                    status_.finish();
                }

                return false;
            }
            else
            {
                LOG_INFO("Start snapshot was on target but end snapshot had different guid");
                boost::this_thread::interruption_point();

                api::restoreSnapshot(target_volume_.get(),
                                     start_snapshot_name);
            }
        }
        else
        {
            LOG_INFO("Start snapshot was on target but end snapshot not");
            boost::this_thread::interruption_point();

            api::restoreSnapshot(target_volume_.get(),
                                 start_snapshot_name);
        }
    }
    else
    {
        LOG_INFO("Backup volume existed but no start snapshot was given... trying to find the best place to backup from");
        LOG_INFO("Getting the list of snapshots from the target");

        std::list<std::string> snapshots_list;
        api::showSnapshots(target_volume_.get(),
                           snapshots_list);

        if(snapshots_list.empty())
        {
            LOG_WARN("No snapshots in backed up volume, something went wrong??");
            LOG_WARN("Checking for a failed first backup because the snapshots list is empty");
            get_target_volume_info();
            if(source_volume_config->getNS().str() != target_volume_config->id_)
            {
                LOG_FATAL("No snapshot on the volume and volume names don't match: "
                          << source_volume_config->getNS() << " vs. " << std::string(target_volume_config->id_));

                throw BackupException("Volume on backup had no snapshots and wrong volume name");
            }
            if(target_volume_config->wan_backup_volume_role_ != VolumeConfig::WanBackupVolumeRole::WanBackupBase)
            {
                LOG_FATAL("Target volume exists, has not snapshot and names match but has the wrong role");
                throw BackupException("Volume on backup had no snapshots and wrong role");
            }
            LOG_WARN("Seems to have been a botched backup, cleaning up");
            BackendInterfacePtr bip = VolManager::get()->createBackendInterface(target_namespace);
            std::list<std::string> objects;
            bip->listObjects(objects);

            BackendNamesFilter is_vd_object;
            for (const auto& o : objects)
            {
                if (is_vd_object(o))
                {
                    LOG_INFO("Removing " << o);
                    bip->remove(o);
                }
            }

            throw RetryCreateVolume();
        }

        std::string snapshot_to_be_restored;

        LOG_INFO("Looping of target snapshotshots to find latest that can be matched");

        for(std::list<std::string>::const_reverse_iterator i = snapshots_list.rbegin();
            i != snapshots_list.rend();
            ++i)
        {
            const std::string& snap_name = *i;
            if(source_snapshot_persistor->snapshotExists(snap_name))
            {
                SnapshotNum num = source_snapshot_persistor->getSnapshotNum(snap_name);
                UUID snap_uuid = source_snapshot_persistor->getUUID(num);
                if(api::checkSnapshotUUID(target_volume_.get(),
                                          snap_name,
                                          snap_uuid))
                {
                    snapshot_to_be_restored = snap_name;
                    break;
                }
                else
                {
                    LOG_FATAL("Snapshot with the same name but different guid found, " << snap_name);
                    throw BackupException("Guid confusion");
                }

            }
        }
        if(snapshot_to_be_restored.empty())
        {
            LOG_FATAL("Could not find a matching snapshot ");
            throw BackupException("Could not find a matching snapshot");
        }
        else if(snapshot_to_be_restored == end_snapshot_name)
        {
            LOG_INFO("snapshot to be restored == end snapshot, exiting early");
            {
                status_.finish();
            }

            return false;
        }

        // Y42 we might be a lot smarter here
        LOG_INFO("Connecting snapshot found as " << snapshot_to_be_restored);

        LOG_INFO("Doing a restore to that snapshot on the target volume");
        boost::this_thread::interruption_point();
        api::restoreSnapshot(target_volume_.get(),
                             snapshot_to_be_restored);
        start_snapshot_ = snapshot_to_be_restored;

    }
    status_.start_snapshot(start_snapshot_.get_value_or(""));

    return true;
}

bool
Backup::create_backup_volume()
{
    LOG_INFO(__FUNCTION__);
    // Need and AssertLocked here
    VERIFY(not target_volume_);
    VERIFY(source_volume_config.get());
    api::getManagementMutex().assertLocked();

    // We check whether the source is a Normal volume
    boost::optional<VolumeConfig::WanBackupVolumeRole> role;
    VERIFY(not role);

    switch(source_volume_config->wan_backup_volume_role_)
    {
    case VolumeConfig::WanBackupVolumeRole::WanBackupNormal:
        if(start_snapshot_)
        {
            LOG_INFO("Incremental backup of a normal volume");
            role = VolumeConfig::WanBackupVolumeRole::WanBackupIncremental;
        }
        else
        {
            LOG_INFO("Complete backup of a normal volume");
            role = VolumeConfig::WanBackupVolumeRole::WanBackupBase;
        }
        break;
    case VolumeConfig::WanBackupVolumeRole::WanBackupBase:
        if(start_snapshot_)
        {
            LOG_INFO("Incremental backup of a backup volume");
            role = VolumeConfig::WanBackupVolumeRole::WanBackupIncremental;
        }
        else
        {
            LOG_INFO("Complete backup of a normal volume");
            role = VolumeConfig::WanBackupVolumeRole::WanBackupBase;
        }
        break;

    case VolumeConfig::WanBackupVolumeRole::WanBackupIncremental:
        if(start_snapshot_)
        {
            LOG_INFO("Incremental backup of a incremental volume");
            role = VolumeConfig::WanBackupVolumeRole::WanBackupIncremental;
        }
        else
        {
            std::vector<SnapshotNum> snaps;
            source_snapshot_persistor->getAllSnapshots(snaps);
            VERIFY(snaps.size() >= 2);
            start_snapshot_ = source_snapshot_persistor->getSnapshotName(snaps[0]);
            LOG_INFO("Complete backup of a incremental volume, created start snapshot " << *start_snapshot_);
            role = VolumeConfig::WanBackupVolumeRole::WanBackupIncremental;
        }
        break;
    }

    VERIFY(role);

    LOG_INFO("Creating new volume in the target with role " << *role);
    try
    {
        boost::this_thread::interruption_point();

        TODO("AR: consider OwnerTag")

        const auto params =
            WriteOnlyVolumeConfigParameters(VolumeId(source_volume_config->getNS().str()),
                                            Namespace(target_namespace),
                                            VolumeSize(source_volume_config->lba_count() *
                                                       source_volume_config->lba_size_),
                                            *role,
                                            OwnerTag(1))
            .lba_size(source_volume_config->lba_size_)
            .cluster_multiplier(source_volume_config->cluster_mult_)
            .sco_multiplier(source_volume_config->sco_mult_);

        target_volume_.reset(api::createNewWriteOnlyVolume(params));
    }
    CATCH_STD_ALL_LOG_RETHROW("problem creating WriteOnlyVolume");

    LOG_INFO("Created new volume in the target -- name is the namespace of the source" << source_volume_config->getNS());

    if(start_snapshot_)
    {
        LOG_INFO("Just created a volume and start snapshot was given... we create a snapshot in the beginning of the volume");
        const SnapshotNum start_snapshot_num =
            source_snapshot_persistor->getSnapshotNum(*start_snapshot_);
        const std::string& start_snapshot_name = *start_snapshot_;

        const UUID
            start_snapshot_uuid(source_snapshot_persistor->getUUID(start_snapshot_num));

        boost::this_thread::interruption_point();
        api::createSnapshot(target_volume_.get(),
                            SnapshotMetaData(),
                            &start_snapshot_name,
                            start_snapshot_uuid);

        WAIT_FOR_THIS_VOLUME_WRITE(target_volume_.get());

        VERIFY(api::isVolumeSyncedUpTo(target_volume_.get(),
                                       start_snapshot_name));
        if(*start_snapshot_ == end_snapshot_name)
        {
            LOG_INFO("start_snapshot == end_snapshot " << end_snapshot_name << " so exiting now");

            while(not api::isVolumeSyncedUpTo(target_volume_.get(),
                                              *start_snapshot_))
            {
                LOG_INFO("Waiting for the volume to finish writing to the backend, sleeping another 10 seconds");
                sleep(10);
            }

            status_.finish();

            LOG_INFO("Finished the Backup");
            return false;
        }
    }
    else
    {
        LOG_INFO("Is a full backup to the newly created volume");
    }
    return true;
}

bool
Backup::try_to_restart_or_create_target()
{
    LOG_INFO(__FUNCTION__);

    LOG_INFO("Trying to restart the target namespace against the target volumedriver");
    try
    {
        fungi::ScopedLock lock_(api::getManagementMutex());
        TODO("AR: consider OwnerTag");
        target_volume_.reset(api::restartVolumeWriteOnly(target_namespace,
                                                         OwnerTag(1)));
    }
    catch(VolManager::VolumeDoesNotHaveCorrectRole&)
    {
        // This is not so good... it explicitly ties volume write only behaviour to the backup case
        LOG_ERROR("the volume at " << target_namespace << " doesn't seem to be a backup volume");
        throw;
    }
    catch(std::exception& e)
    {
        fungi::ScopedLock lock_(api::getManagementMutex());
        return create_backup_volume();
    }
    try
    {
        // we get here when the volume was restarted
        fungi::ScopedLock lock_(api::getManagementMutex());
        return preexisting_volume_checks();
    }
    catch(RetryCreateVolume&)
    {
        VERIFY(target_volume_);
        LOG_INFO("Createing the volume anew after cleanup");
        target_volume_.reset(0);
        fungi::ScopedLock lock_(api::getManagementMutex());
        return create_backup_volume();
    }
}
namespace
{
struct BackupAccumulator
{
    DECLARE_LOGGER("BackupAccumulator");

    BackupAccumulator(volumedriver::CloneTLogs& clone_tlogs,
                      volumedriver::NSIDMap& nsid,
                      uint64_t& size)
        : clone_tlogs_(clone_tlogs)
        , nsid_(nsid)
        , size_(size)

    {
        VERIFY(nsid_.size() == 1);
    }

    void
    operator()(const volumedriver::SnapshotPersistor& sp,
               const BackendInterfacePtr& bi,
               const std::string& snapshot_name,
               const volumedriver::SCOCloneID clone_id)
    {
        OrderedTLogNames tlogs;
        sp.getTLogsTillSnapshot(snapshot_name, tlogs);
        clone_tlogs_.push_back(std::make_pair(clone_id,
                                              std::move(tlogs)));
        size_ += sp.getBackendSize(snapshot_name,
                                   boost::none);
        if(clone_id != SCOCloneID(0))
        {
            nsid_.set(clone_id,
                      bi->clone());
        }
    }


    volumedriver::CloneTLogs& clone_tlogs_;
    volumedriver::NSIDMap& nsid_;
    uint64_t& size_;
    static const FromOldest direction = FromOldest::T;
};


}

void
Backup::get_tlogs_to_replay()
{
    LOG_INFO(__FUNCTION__);

    VERIFY(not start_snapshot_ or
           start_snapshot_ != end_snapshot_name);
    VERIFY(source_snapshot_persistor);
    if(not start_snapshot_)
    {
        // Might be wrong if we have snapshot negotiation!
        LOG_INFO("No start snapshot given we do a full backup of the source to the end snapshot");

        BackupAccumulator ba(replayTLogs,
                             nsid,
                             total_size_);
        source_snapshot_persistor->vold(ba,
                                        nsid.get(0)->clone(),
                                        end_snapshot_name);

        LOG_INFO("total size to backup " << total_size_);
        //replayTLogs.push_back(std::make_pair(volumedriver::SCOCloneID(0),recentTLogs));
    }
    else if (*start_snapshot_ != end_snapshot_name)
    {
        LOG_INFO("Start snapshot given, we do a backup from snapshot " << *start_snapshot_ << " to " << end_snapshot_name);

        const SnapshotNum start_snapshot_num = source_snapshot_persistor->getSnapshotNum(*start_snapshot_);
        OrderedTLogNames recentTLogs;
        source_snapshot_persistor->getTLogsBetweenSnapshots(start_snapshot_num,
                                                            end_snapshot_number,
                                                            recentTLogs);
        total_size_ += source_snapshot_persistor->getBackendSize(end_snapshot_name,
                                                                 start_snapshot_);
        LOG_INFO("total size to backup " << total_size_);

        replayTLogs.push_back(std::make_pair(volumedriver::SCOCloneID(0), recentTLogs));
    }

}

void
Backup::replay_tlogs_on_target()
{
    LOG_INFO("write_tlogs_to_backend()");
    VERIFY(not start_snapshot_ or
           start_snapshot_ != end_snapshot_name);
    VERIFY(target_volume_ != 0);
    {

        fungi::ScopedLock lock_(api::getManagementMutex());

        LOG_INFO("Attaching target volume");

        //        api::attachVolume(target_volume_.get());
    }

    uint64_t bitset_size =  source_volume_config->lba_count() / source_volume_config->cluster_mult_;

    boost::dynamic_bitset<> cluster_bitset(bitset_size);

    const fs::path tlogDir = file_pool->directory() / "tlogs";

    const uint64_t cluster_size = source_volume_config->lba_size_ * source_volume_config->cluster_mult_;

    FileUtils::checkDirectoryEmptyOrNonExistant(tlogDir);
    const fs::path the_sco = file_pool->newFile("the_sco");
    // Y42
    const unsigned MAX_SCO_SIZE = 3 * source_volume_config->sco_mult_
        * source_volume_config->cluster_mult_ * source_volume_config->lba_size_;
    std::vector<byte> buf(MAX_SCO_SIZE);


    //    VERIFY(source_snapshot_persistor);
    // const uint64_t total_size(source_snapshot_persistor->getBackendSize(end_snapshot_name,
    //                                                                     start_snapshot_));

    LOG_INFO("Starting the progress reporting to the target backend");

    status_.start(target_volume_.get(),
                  total_size_);

    LOG_INFO("Replaying the source tlogs on the target volume");

    // This has to change... there is no guarantee that the clonetlogs are filled up correctly
    // other than most of our code does is... should be a map
    for(CloneTLogs::const_reverse_iterator i = replayTLogs.rbegin();
        i != replayTLogs.rend();
        ++i)
    {
        LOG_INFO("Working on Clone " << (int)(i->first));

        VERIFY(i->first < nsid.size());
        uint64_t usleep_micro_sec = 100000;
        const uint64_t ten_seconds = 10000000;

        for(OrderedTLogNames::const_reverse_iterator tlog_it = i->second.rbegin();
            tlog_it != i->second.rend();
            ++tlog_it)
        {
            LOG_INFO("Working on TLog " << *tlog_it);
            status_.current_tlog(*tlog_it);

            std::unique_ptr<TLogReaderInterface>
                tlog_reader(new volumedriver::BackwardTLogReader(tlogDir,
                                                                 *tlog_it,
                                                                 nsid.get(i->first)->clone()));

            SCO current_sco;
            uint64_t current_sco_size = 0;

            const Entry* entry = 0;
            while((entry = tlog_reader->nextLocation()))
            {

                boost::this_thread::interruption_point();
                // Y42 do we do CRC checking here?
                if(entry->isLocation())
                {
                    status_.add_seen();

                    const ClusterAddress cluster_address = entry->clusterAddress();
                    VERIFY(cluster_address < bitset_size);
                    if(not cluster_bitset.test(cluster_address))
                    {
                        status_.add_kept();

                        const ClusterLocation cluster_location(entry->clusterLocation());
                        const SCO looking_at_sco(cluster_location.sco());

                        if(looking_at_sco != current_sco)
                        {
                            nsid.get(i->first)->read(the_sco,
                                                     looking_at_sco.str(),
                                                     InsistOnLatestVersion::F);
                            ALWAYS_CLEANUP_FILE(the_sco);

                            current_sco_size = fs::file_size(the_sco);
                            VERIFY(current_sco_size <= buf.size());
                            youtils::FileDescriptor sio(the_sco, youtils::FDMode::Read);
                            const ssize_t res = sio.read(&buf[0], current_sco_size);
                            VERIFY(res == static_cast<ssize_t>(current_sco_size));
                            current_sco = looking_at_sco;
                        }

                        VERIFY((cluster_location.offset() + 1) * cluster_size <= current_sco_size);
                        boost::this_thread::interruption_point();
                        bool finished = false;
                        while(not finished)
                        {
                            try
                            {
                                api::Write(target_volume_.get(),
                                           cluster_address << 3,
                                           &buf[0] + (cluster_location.offset() * cluster_size),
                                           cluster_size);
                                finished = true;
                            }
                            catch(TransientException& e)
                            {
                                LOG_INFO("Caught a transient exception while writing, sleeping for "
                                         << usleep_micro_sec << " microseconds");
                                usleep(usleep_micro_sec);
                                if(usleep_micro_sec < ten_seconds)
                                {
                                    usleep_micro_sec = std::min(usleep_micro_sec * 2, ten_seconds);
                                }
                            }
                        }

                        cluster_bitset[cluster_address] = 1;
                    }
                }
            }
        }
    }

    LOG_INFO("Waiting for all writes to hit the backend");
    boost::this_thread::interruption_point();
    WAIT_FOR_THIS_VOLUME_WRITE(target_volume_.get());

    LOG_INFO("Finished writing to the backend, will create a snapshot now with name source end snapshot: "
              << end_snapshot_name);

    UUID end_snapshot_guid(source_snapshot_persistor->getUUID(end_snapshot_number));
    boost::this_thread::interruption_point();

    {
        fungi::ScopedLock lock_(api::getManagementMutex());
        LOG_INFO("Creating last snapshot " << end_snapshot_name);
        api::createSnapshot(target_volume_.get(),
                            SnapshotMetaData(),
                            &end_snapshot_name,
                            end_snapshot_guid);
    }

    status_.current_tlog("");

    WAIT_FOR_THIS_VOLUME_WRITE(target_volume_.get());
    {
        fungi::ScopedLock lock_(api::getManagementMutex());

        VERIFY(api::isVolumeSyncedUpTo(target_volume_.get(),
                                       end_snapshot_name));

        LOG_INFO("Detaching target volume");

        //        api::detachVolume(target_volume_.get());
    }
    status_.finish();

    LOG_INFO("Detached Volume, now Exiting");
}

std::string
Backup::info()
{
    return "Backup running";
}

void
Backup::operator()()
{
    ApiInitializer api(target_ptree);

    BOOST_SCOPE_EXIT((&target_volume_))
    {
        target_volume_.reset(0);
    }
    BOOST_SCOPE_EXIT_END;

    LOG_INFO("Starting the Backup");
    std::stringstream ss;

    boost::property_tree::write_info(ss, configuration_ptree_);
    LOG_INFO("Parameters passed : \n" << ss.str());

    setup_file_pool();
    get_source_snapshots_file();
    figure_out_end_snapshot();
    figure_out_start_snapshot();

    get_source_volume_info();

    if(not try_to_restart_or_create_target())
    {
        return;
    }

    get_tlogs_to_replay();
    replay_tlogs_on_target();

    LOG_INFO("Finished the Backup");
}

}

// Local Variables: **
// mode: c++ **
// End: **
