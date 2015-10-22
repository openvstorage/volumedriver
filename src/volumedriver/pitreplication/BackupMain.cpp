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

#include "../Backup.h"
#include "../DeleteSnapshot.h"
#include "../VolManager.h"

#include <iostream>

#include <boost/program_options.hpp>

#include <youtils/BuildInfoString.h>
#include <youtils/HeartBeatLockService.h>
#include <youtils/Logger.h>
#include <youtils/Main.h>
#include <youtils/NoGlobalLockingService.h>
#include <youtils/WithGlobalLock.h>

#include <backend/LockStore.h>

namespace
{

namespace be = backend;
namespace po = boost::program_options;
namespace vd = volumedriver;
namespace vd_bu = volumedriver_backup;
namespace yt = youtils;

using LockService = yt::HeartBeatLockService;

class BackupMain
    : public yt::MainHelper
{
public:
    BackupMain(int argc,
               char** argv)
        : MainHelper(argc,
                     argv)
        , backup_options_("Backup Options")
    {
        backup_options_.add_options()
            ("delete-snapshot",
             po::value<std::vector<std::string>>(&snapshots_)->composing(),
             "Delete the named snapshot")
            ("configuration-file",
             po::value<std::string>(&configuration_file_)->required(),
             "File that holds the configuration for this job");
    }

    virtual void
    log_extra_help(std::ostream& strm) override final
    {
        strm << backup_options_;
    }

    virtual void
    parse_command_line_arguments() override final
    {
        parse_unparsed_options(backup_options_,
                               yt::AllowUnregisteredOptions::T,
                               vm_);
    }

    virtual void
    setup_logging() override final
    {
        MainHelper::setup_logging();
    }

    virtual int
    run() override final
    {
        boost::property_tree::ptree config_ptree;
        boost::property_tree::json_parser::read_json(configuration_file_,
                                                     config_ptree);

        boost::property_tree::ptree& target_ptree = config_ptree.get_child("target_configuration");

        auto bcm(be::BackendConnectionManager::create(target_ptree));

        {
            //[BDV] bit hackish as a useless parameter should not be configurable in the first place
            //checking here already instead of within Backup as otherwize we might waste time
            //stealing a lock
            initialized_params::PARAMETER_TYPE(serialize_read_cache) serialize_read_cache(target_ptree);
            if (serialize_read_cache.value())
            {
                throw vd_bu::BackupException("readcache serialization should be disabled in the config file");
            }
        }

        if(vm_.count("delete-snapshot"))
        {
            const std::string target_namespace = config_ptree.get_child("target_configuration").get<std::string>("namespace");
            const uint64_t update_interval = config_ptree.get<uint64_t>("global_lock_update_interval_in_seconds", 30);

            LOG_INFO("lock update interval" << update_interval);

            std::vector<vd::SnapshotName> snaps;
            snaps.reserve(snapshots_.size());
            for (const auto& s : snapshots_)
            {
                snaps.emplace_back(vd::SnapshotName(s));
            }

            vd_bu::DeleteSnapshot deleter(config_ptree,
                                          snaps);

            using LockedDeleter =
                LockService::WithGlobalLock<yt::ExceptionPolicy::ThrowExceptions,
                                            vd_bu::DeleteSnapshot,
                                            &vd_bu::DeleteSnapshot::info>;

            yt::GlobalLockStorePtr
                lock_store(new be::LockStore(bcm->newBackendInterface(be::Namespace(target_namespace))));

            LockedDeleter locked_deleter(boost::ref(deleter),
                                         yt::NumberOfRetries(1),
                                         LockedDeleter::connection_retry_timeout_default(),
                                         lock_store,
                                         yt::UpdateInterval(boost::posix_time::seconds(update_interval)));

            LOG_INFO("Starting Locked Snapshot Deletion");
            try
            {
                locked_deleter();
            }
            catch(yt::WithGlobalLockExceptions::CouldNotInterruptCallable& e)
            {
                LOG_FATAL("Aborting because the worker thread could not be interrupted and we *don't* have the lock anymore");
                exit(1);
            }

            catch(yt::WithGlobalLockExceptions::CallableException& e)
            {
                std::rethrow_exception(e.exception_);
            }

            LOG_INFO("Finished Locked Snapshot Deletion");
            return 0;
        }
        else
        {

            const std::string target_namespace = config_ptree.get_child("target_configuration").get<std::string>("namespace");
            const uint64_t update_interval = config_ptree.get<uint64_t>("global_lock_update_interval_in_seconds", 30);
            LOG_INFO("lock update interval" << update_interval);

            vd_bu::Backup backup(config_ptree);

            using LockedBackup =
                LockService::WithGlobalLock<yt::ExceptionPolicy::ThrowExceptions,
                                            vd_bu::Backup,
                                            &vd_bu::Backup::info>;
            yt::GlobalLockStorePtr
                lock_store(new be::LockStore(bcm->newBackendInterface(be::Namespace(target_namespace))));

            LockedBackup locked_backup(boost::ref(backup),
                                       yt::NumberOfRetries(1),
                                       LockedBackup::connection_retry_timeout_default(),
                                       lock_store,
                                       yt::UpdateInterval(boost::posix_time::seconds(update_interval)));

            LOG_INFO("Starting Locked Backup");

            try
            {
                locked_backup();
            }
            catch(yt::WithGlobalLockExceptions::CouldNotInterruptCallable& e)
            {
                LOG_FATAL("Aborting because the worker thread could not be interrupted and we *don't* have the lock anymore");
                exit(1);
            }

            catch(yt::WithGlobalLockExceptions::CallableException& e)
            {
                std::rethrow_exception(e.exception_);
            }

            LOG_INFO("Finished Locked Backup");
            return 0;
        }
    }

    std::string configuration_file_;
    po::options_description backup_options_;
    std::vector<std::string> snapshots_;
};

}

MAIN(BackupMain)


// Local Variables: **
// mode: c++ **
// End: **
