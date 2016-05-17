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

#include "Restore.h"

#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <backend/BackendInterface.h>
#include <backend/BackendConnectionManager.h>

#include <volumedriver/SnapshotPersistor.h>
#include <volumedriver/SnapshotManagement.h>
#include <volumedriver/TLogReader.h>

namespace volumedriver_backup
{

// move to youtils/Catchers
#define LOG_N_THROW(exception_type, loglevel, msg)                      \
    {                                                                   \
        std::stringstream ss;                                           \
        ss << msg;                                                      \
        LOG_##loglevel(ss.str());                                       \
        throw exception_type(ss.str().c_str());                         \
    }

Restore::Restore(const fs::path& configuration_file,
                 bool barf_on_busted_backup,
                 const youtils::GracePeriod& grace_period)
    : configuration_file_(configuration_file)
    , scratch_dir_(vd::FileUtils::temp_path())
    , barf_on_busted_backup_(barf_on_busted_backup)
    , grace_period_(grace_period)
{}

void
Restore::print_configuration_documentation(std::ostream& os)
{
    os << "Restores a backed up volume (called the source) to a new namespace (target) so it's backend restartable" << std::endl;
}

std::string
Restore::info()
{
    // More info here
    return "restoring";
}

void
Restore::operator()()
{
    LOG_INFO("Starting the Restore");

    bpt::ptree total_pt;
    bpt::json_parser::read_json(configuration_file_.string(),
                                total_pt);

    init_(total_pt);

    const bpt::ptree& target_pt = total_pt.get_child("target_configuration");

    be::BackendConnectionManagerPtr
        target_cm(be::BackendConnectionManager::create(target_pt,
                                                       RegisterComponent::F));

    const std::string target_nspace(target_pt.get<std::string>("namespace"));

    be::BackendInterfacePtr
        target_bi(target_cm->newBackendInterface(backend::Namespace(target_nspace)));

    const boost::optional<bpt::ptree&>
        maybe_source_pt(total_pt.get_child_optional("source_configuration"));

    vd::VolumeConfig vol_config;

    if (maybe_source_pt)
    {
        const std::string source_ns((*maybe_source_pt).get<std::string>("namespace"));

        be::BackendConnectionManagerPtr
            source_cm(be::BackendConnectionManager::create(*maybe_source_pt,
                                                           RegisterComponent::F));

        be::BackendInterfacePtr source_bi(source_cm->newBackendInterface(backend::Namespace(source_ns)));
        copy_(source_bi->clone(), target_bi->clone());

        LOG_INFO("Retrieving volume config from source namespace " <<
                 source_bi->getNS());
        source_bi->fillObject(vol_config, InsistOnLatestVersion::T);
        vol_config.changeNamespace(backend::Namespace(target_nspace));
    }
    else
    {
        LOG_INFO("Retrieving volume config from target namespace " <<
                 target_bi->getNS());
        target_bi->fillObject(vol_config, InsistOnLatestVersion::T);
    }

    const boost::optional<std::string>
        maybe_new_name(target_pt.get_optional<std::string>("volume_name"));

    if (maybe_new_name)
    {
        LOG_INFO(target_nspace << ": renaming " << vol_config.id_ << " -> " <<
                 *maybe_new_name);
        const_cast<vd::VolumeId&>(vol_config.id_) = vd::VolumeId(*maybe_new_name);
    }

    const boost::optional<vd::VolumeConfig::WanBackupVolumeRole>
        maybe_new_role(target_pt.get_optional<vd::VolumeConfig::WanBackupVolumeRole>("volume_role"));

    if (maybe_new_role)
    {
        promote_(*maybe_new_role, target_bi->clone(), vol_config);
    }

    target_bi->writeObject(vol_config,
                           vd::VolumeConfig::config_backend_name,
                           OverwriteObject::T);
}

void
Restore::init_(const bpt::ptree& pt)
{
    std::stringstream ss;

    bpt::write_info(ss, pt);
    LOG_INFO("Parameters passed:\n" << ss.str());

    const boost::optional<fs::path>
        maybe_scratch_dir(pt.get_optional<fs::path>("scratch_dir"));

    if (maybe_scratch_dir)
    {
        scratch_dir_ = *maybe_scratch_dir;
    }

    LOG_INFO("Using " << scratch_dir_ << " as scratch directory");


}

void
Restore::copy_(be::BackendInterfacePtr source_bi,
               be::BackendInterfacePtr target_bi)
{
    LOG_INFO("Copying volume from " << source_bi->getNS() << " -> " <<
             target_bi->getNS());

    if (not source_bi->objectExists(vd::VolumeConfig::config_backend_name))
    {
        LOG_N_THROW(RestoreException,
                    ERROR,
                    source_bi->getNS() <<
                    ": no volume config found in source namespace");
    }

    std::list<std::string> objects;
    source_bi->listObjects(objects);

    // Y42 straight copy would save some disk IO's
    // -- Almost straight namespace - namespace copy
    for (const auto& object : objects)
    {
        if(object != vd::VolumeConfig::config_backend_name and
           object != "lock_name")
        {
            const fs::path p(vd::FileUtils::create_temp_file_in_temp_dir(object));
            ALWAYS_CLEANUP_FILE(p);
            boost::this_thread::interruption_point();
            source_bi->read(p,
                            object,
                            InsistOnLatestVersion::T);
            target_bi->write(p,
                             object);
        }
    }
}

void
Restore::promote_(vd::VolumeConfig::WanBackupVolumeRole new_role,
                  be::BackendInterfacePtr bi,
                  vd::VolumeConfig& cfg)
{
    const vd::VolumeConfig::WanBackupVolumeRole old_role =
        cfg.wan_backup_volume_role_;
    LOG_INFO(bi->getNS() << ": promoting " << cfg.id_.str() <<
             " " << cfg.wan_backup_volume_role_ << " -> " <<
             new_role);

    if (old_role == new_role)
    {
        LOG_INFO(cfg.id_ << ": already in desired role - not doing anything");
    }
    else if (old_role == vd::VolumeConfig::WanBackupVolumeRole::WanBackupBase and
             new_role == vd::VolumeConfig::WanBackupVolumeRole::WanBackupNormal)
    {
        sanitize_snapshots_(bi->clone());
        const_cast<vd::VolumeConfig::WanBackupVolumeRole&>(cfg.wan_backup_volume_role_) = new_role;
    }
    else
    {
        LOG_N_THROW(WrongVolumeRole,
                    ERROR,
                    bi->getNS() << ": " << cfg.id_.str() <<
                    " cannot be promoted: invalid transition " <<
                    cfg.wan_backup_volume_role_ << " -> " << new_role);
    }
}

void
Restore::sanitize_snapshots_(be::BackendInterfacePtr bi) const
{
    LOG_INFO(bi->getNS() << ": reading snapshots file to check for partial backups");

#define FAILED_BACKUP(msg)                              \
    LOG_N_THROW(FailedBackupException, ERROR, msg)

    auto sp(vd::SnapshotManagement::createSnapshotPersistor(bi->clone()));

    std::vector<vd::SnapshotNum> snap_nums;
    sp->getAllSnapshots(snap_nums);

    if (snap_nums.empty())
    {
        FAILED_BACKUP("no snapshots found");
    }

    bool current_tlog_ok = false;
    vd::TLogId current_tlog(yt::UUID::NullUUID());

    {
        const vd::OrderedTLogIds current_tlogs(sp->getCurrentTLogs());

        current_tlog_ok = current_tlogs.size() == 1 and
            not sp->isTLogWrittenToBackend(current_tlogs[0]);
        if (current_tlog_ok)
        {
            current_tlog = current_tlogs[0];
        }
    }

    vd::SnapshotNum snap_num;

    if (not sp->isSnapshotInBackend(snap_nums.back()))
    {
        LOG_WARN(bi->getNS() << ": last snapshot not in backend");
        if (snap_nums.size() == 1)
        {
            FAILED_BACKUP("only one snapshot found and that one is not in the backend");
        }

        vd::OrderedTLogIds tlogs;
        sp->getTLogsAfterSnapshot(snap_nums.back(), tlogs);
        if (not tlogs.empty())
        {
            if (current_tlog_ok and tlogs.size() == 1)
            {
                VERIFY(current_tlog == tlogs[0]);
            }
            else
            {
                // print out the tlogs as a courtesy to users - if anything goes wrong
                // during our garbage collection users can resume manually.
                for (const auto& tlog : tlogs)
                {
                    LOG_ERROR(bi->getNS() << ": trailing tlog " << tlog);
                }

                FAILED_BACKUP("trailing tlogs after a snapshot not in backend");
            }
        }

        if (barf_on_busted_backup_)
        {
            FAILED_BACKUP("last snapshot not in backend");
        }

        ASSERT(snap_nums.size() >= 2);
        snap_num = snap_nums[snap_nums.size() - 2];
    }
    else
    {
        ASSERT(snap_nums.size() >= 1);
        snap_num = snap_nums[snap_nums.size() - 1];
    }

    LOG_INFO(bi->getNS() << ": restoring to snapshot " << snap_num <<
             ", named " << sp->getSnapshotName(snap_num));

    vd::OrderedTLogIds doomed_tlogs;
    sp->getTLogsAfterSnapshot(snap_num, doomed_tlogs);

    // We could be more rigid: snapshot creation will always create a new empty
    // tlog.
    if (not doomed_tlogs.empty())
    {
        if (current_tlog_ok and doomed_tlogs.size() == 1)
        {
            VERIFY(current_tlog == doomed_tlogs[0]);
            LOG_INFO(bi->getNS() << ": we have a perfectly valid backup");
            doomed_tlogs.clear();
            // extra sanity: verify that the current tlog is empty?
        }
        else if (barf_on_busted_backup_)
        {
            FAILED_BACKUP("trailing tlogs found");
        }
    }

    // This will also rid us of empty tlogs automatically created after snapshotting.
    // The SnapshotPersistor is responsible (and currently does so) for creating a
    // new TLog after deleting the surplus tlogs and snapshots.
    sp->deleteTLogsAndSnapshotsAfterSnapshot(snap_num);

    // XXX:
    // Writing out the snapshots marks the point of no return - if we crash after
    // that the doomed TLogs + the SCOs referenced by them are leaked. This needs
    // to be conveyed to users.
    LOG_INFO(bi->getNS() << ": writing out snapshots file");
    vd::SnapshotManagement::writeSnapshotPersistor(*sp, bi->clone());

    if (not doomed_tlogs.empty())
    {
        collect_garbage_(bi->clone(), doomed_tlogs);
    }

#undef FAILED_BACKUP
}

void
Restore::collect_garbage_(be::BackendInterfacePtr bi,
                          const vd::OrderedTLogIds& doomed_tlogs) const
{
    LOG_INFO(bi->getNS() << ": collecting garbage");

    for (const auto& tlog_id : doomed_tlogs)
    {
        const auto tlog_name(boost::lexical_cast<std::string>(tlog_id));

        try
        {
            std::unique_ptr<vd::TLogReaderInterface>
                r(new vd::TLogReader(scratch_dir_,
                                     tlog_name,
                                     bi->clone()));

            std::vector<vd::SCO> doomed_scos;
            r->SCONames(doomed_scos);

            LOG_INFO(bi->getNS() << ": removing surplus tlog " << tlog_name);
            try
            {
                bi->remove(tlog_name,
                           ObjectMayNotExist::T);
            }
            CATCH_STD_ALL_LOGLEVEL_IGNORE(bi->getNS() << ": failed to remove " << tlog_name,
                                          WARN);


            for (const auto& sco : doomed_scos)
            {
                LOG_INFO(bi->getNS() << ": removing surplus sco " << sco);
                try
                {
                    bi->remove(sco.str(), ObjectMayNotExist::T);
                }
                CATCH_STD_ALL_LOGLEVEL_IGNORE(bi->getNS() << ": failed to remove " <<
                                              sco,
                                              WARN);
            }
        }
        CATCH_STD_ALL_LOGLEVEL_IGNORE(bi->getNS() <<
                                      ": failed to process tlog " << tlog_name <<
                                      " - it probably didn't make it to the backend?",
                                      WARN);
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
