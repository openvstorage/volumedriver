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

#ifndef VOLUMEDRIVER_BACKUP_BACKUP_H_
#define VOLUMEDRIVER_BACKUP_BACKUP_H_

#include "SnapshotPersistor.h"
#include "StatusWriter.h"

#include <youtils/Logging.h>

#include <backend/BackendConnectionManager.h>
// Probably remove this here!
#include <youtils/WithGlobalLock.h>

#include <volumedriver/Api.h>
#include <volumedriver/FilePool.h>
#include <volumedriver/ScopedVolume.h>
#include <volumedriver/VolumeConfig.h>

namespace volumedriver_backup
{

class BackupException : public std::exception
{
public:
    BackupException(const std::string& message) throw()
        : message_(message)
    {}
    ~BackupException() throw()
    {}

    virtual const char* what() const throw()
    {
        return message_.c_str();
    }
private:
    std::string message_;

};


class Backup
    : public youtils::GlobalLockedCallable
{
public:
    explicit Backup(const std::string& configuration_file);


    explicit Backup(const boost::property_tree::ptree&);

    static void
    print_configuration_documentation(std::ostream& os);

    void
    operator()();

    DECLARE_LOGGER("Backup");

    struct ApiInitializer
    {
        ApiInitializer(const boost::property_tree::ptree& pt)
        {
             LOG_TRACE("Starting the volmanager against the target configuration");
             api::Init(pt);
        }

        ~ApiInitializer()
        {
            LOG_TRACE("Stopping the volmanager");
            api::Exit();
            LOG_TRACE("Finished stopping the volmanager");
        }
    };

    const youtils::GracePeriod&
    grace_period() const
    {
        return grace_period_;
    }

    std::string
    info();

private:
    void
    figure_out_end_snapshot();

    void
    figure_out_start_snapshot();

    void
    setup_file_pool();

    void
    get_source_snapshots_file();

    void
    get_source_volume_info();

    void
    get_tlogs_to_replay();

    bool
    try_to_restart_or_create_target();

    bool
    preexisting_volume_checks();

    bool
    create_backup_volume();

    void
    replay_tlogs_on_target();

    void
    get_target_volume_info();

    boost::property_tree::ptree configuration_ptree_;
    const boost::property_tree::ptree& source_ptree;
    const boost::property_tree::ptree& target_ptree;
    //    ApiInitializer api;

    const ::volumedriver::Namespace target_namespace;

    volumedriver::SnapshotNum end_snapshot_number;
    volumedriver::SnapshotName end_snapshot_name;

    boost::optional<volumedriver::SnapshotName> start_snapshot_;
    ::volumedriver::CloneTLogs replayTLogs;
    volumedriver::ScopedWriteOnlyVolumeWithLocalDeletion target_volume_;
    std::unique_ptr< ::volumedriver::VolumeConfig> target_volume_config;

    volumedriver::NSIDMap nsid;
    backend::BackendConnectionManagerPtr source_connection_manager;
    std::unique_ptr< ::volumedriver::SnapshotPersistor> source_snapshot_persistor;
    std::unique_ptr< ::volumedriver::FilePool> file_pool;
    std::unique_ptr< ::volumedriver::VolumeConfig> source_volume_config;
    const uint64_t report_threshold;
    StatusWriter status_;
    static const uint64_t report_times_default_;

public:
    const youtils::GracePeriod grace_period_;
    uint64_t total_size_;

};

}

#endif // VOLUMEDRIVER_BACKUP_BACKUP_H_

// Local Variables: **
// mode: c++ **
// End: **
