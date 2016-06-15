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

#ifndef META_DATA_SERVER_MANAGER_H_
#define META_DATA_SERVER_MANAGER_H_

#include "DataBase.h"
#include "Parameters.h"
#include "ServerConfig.h"

#include <chrono>

#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/thread/thread.hpp>

#include <youtils/Logger.h>
#include <youtils/PeriodicActionPool.h>
#include <youtils/VolumeDriverComponent.h>

#include <backend/BackendConnectionManager.h>

namespace volumedrivertest
{

class MDSTestSetup;

}

namespace metadata_server
{

class ServerNG;

class Manager
    : public youtils::VolumeDriverComponent
{
public:
    using Ptr = std::shared_ptr<Manager>;

    Manager(const boost::property_tree::ptree& pt,
            const RegisterComponent registerizle,
            backend::BackendConnectionManagerPtr cm);

    virtual ~Manager();

    Manager(const Manager&) = delete;

    // a reference to mds_poll_secs is used by the servers!
    Manager(Manager&&) = delete;

    Manager&
    operator=(const Manager&) = delete;

    // a reference to mds_poll_secs is used by the servers!
    Manager&
    operator==(Manager&&) = delete;

    DataBaseInterfacePtr
    find(const volumedriver::MDSNodeConfig&) const;

    void
    start_one(const ServerConfig&);

    void
    stop_one(const volumedriver::MDSNodeConfig&);

    ServerConfigs
    server_configs() const;

    // VolumeDriverComponent
    virtual const char*
    componentName() const
    {
        return initialized_params::mds_component_name;
    }

    virtual void
    update(const boost::property_tree::ptree& pt,
           youtils::UpdateReport& report);

    virtual void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault) const;

    virtual bool
    checkConfig(const boost::property_tree::ptree& pt,
                youtils::ConfigurationReport& report) const;

    std::chrono::seconds
    poll_interval() const;

    MAKE_EXCEPTION(Exception,
                   fungi::IOException);

private:
    DECLARE_LOGGER("MetaDataManager");

    DECLARE_PARAMETER(mds_poll_secs);
    DECLARE_PARAMETER(mds_threads);
    DECLARE_PARAMETER(mds_timeout_secs);
    DECLARE_PARAMETER(mds_cached_pages);
    DECLARE_PARAMETER(mds_bg_threads);

    using ServerPtr = std::shared_ptr<ServerNG>;
    using ConfigsAndServers = std::vector<std::pair<ServerConfig, ServerPtr>>;

    backend::BackendConnectionManagerPtr cm_;
    youtils::PeriodicActionPool::Ptr act_pool_;

    // protects nodes_
    mutable boost::mutex lock_;

    ConfigsAndServers nodes_;

    boost::optional<std::string>
    check_configs_(const ServerConfigs&) const;

    ServerPtr
    make_server_(const ServerConfig&) const;

    ConfigsAndServers
    make_nodes_(const ServerConfigs&) const;

    void
    log_nodes_(const char* desc) const;
};

}

#endif // !META_DATA_MANAGER_H_
