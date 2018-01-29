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

#include "Alba_Connection.h"
#include "BackendConfig.h"
#include "BackendConnectionManager.h"
#include "BackendInterface.h"
#include "BackendParameters.h"
#include "BackendSinkInterface.h"
#include "Local_Connection.h"
#include "NamespacePoolSelector.h"
#include "RoundRobinPoolSelector.h"
#include "MultiConfig.h"
#include "S3_Connection.h"

#include <numeric>

#include <boost/iostreams/stream.hpp>
#include <boost/thread/thread.hpp>

#include <youtils/Assert.h>

namespace backend
{

namespace bc = boost::chrono;
namespace bpt = boost::property_tree;
namespace bio = boost::iostreams;
namespace ip = initialized_params;
namespace yt = youtils;

BackendConnectionManager::BackendConnectionManager(const bpt::ptree& pt,
                                                   const RegisterComponent registerize)
    : VolumeDriverComponent(registerize,
                            pt)
    , params_(pt)
    , config_(BackendConfig::makeBackendConfig(pt))
    , next_pool_(0)
{
    THROW_UNLESS(config_);

    const size_t pool_capacity = params_.backend_connection_pool_capacity.value();
    const std::atomic<uint32_t>& blacklist_secs = params_.backend_connection_pool_blacklist_secs.value();

    if (config_->backend_type.value() == BackendType::MULTI)
    {
        auto& cfg = dynamic_cast<const MultiConfig&>(*config_);
        connection_pools_.reserve(cfg.configs_.size());
        for (const auto& c : cfg.configs_)
        {
            connection_pools_.push_back(ConnectionPool::create(c->clone(),
                                                               pool_capacity,
                                                               blacklist_secs));
        }
    }
    else
    {
        connection_pools_.push_back(ConnectionPool::create(config_->clone(),
                                                           pool_capacity,
                                                           blacklist_secs));
    }

    THROW_WHEN(connection_pools_.empty());
}

BackendConnectionManagerPtr
BackendConnectionManager::create(const boost::property_tree::ptree& pt,
                                 const RegisterComponent registrate)
{
    return std::make_shared<yt::EnableMakeShared<BackendConnectionManager>>(pt,
                                                                            registrate);
}

BackendConnectionManager::ConnectionPoolPtr
BackendConnectionManager::pool(const Namespace& nspace) const
{
    return NamespacePoolSelector(*this,
                                 nspace).pool();
}

BackendConnectionInterfacePtr
BackendConnectionManager::getConnection(const ForceNewConnection force_new,
                                        const boost::optional<Namespace>& nspace)
{
    ASSERT(not connection_pools_.empty());

    if (nspace)
    {
        return pool(*nspace)->get_connection(force_new);
    }
    else
    {
        return pools()[0]->get_connection(force_new);
    }
}

size_t
BackendConnectionManager::capacity() const
{
    return std::accumulate(connection_pools_.begin(),
                           connection_pools_.end(),
                           0,
                           [](size_t n, const std::shared_ptr<ConnectionPool>& p)
                           {
                               return n + p->capacity();
                           });
}

size_t
BackendConnectionManager::size() const
{
    return std::accumulate(connection_pools_.begin(),
                           connection_pools_.end(),
                           0,
                           [](size_t n, const std::shared_ptr<ConnectionPool>& p)
                           {
                               return n + p->size();
                           });
}

BackendInterfacePtr
BackendConnectionManager::newBackendInterface(const Namespace& nspace)
{
    return BackendInterfacePtr(new BackendInterface(nspace,
                                                    shared_from_this()));
}

void
BackendConnectionManager::persist(bpt::ptree& pt,
                                  const ReportDefault report_default) const
{
    config_->persist_internal(pt,
                              report_default);
    params_.persist(pt,
                    report_default);
}

void
BackendConnectionManager::update(const bpt::ptree& pt,
                                 yt::UpdateReport& report)
{
    config_->update_internal(pt, report);

    const decltype(params_.backend_connection_pool_capacity) new_cap(pt);

    for (auto& p : connection_pools_)
    {
        p->capacity(new_cap.value());
    }

    params_.update(pt,
                   report);
}

bool
BackendConnectionManager::checkConfig(const bpt::ptree& pt,
                                      yt::ConfigurationReport& rep) const
{
    return config_->checkConfig_internal(pt,
                                         rep);
}

const char*
BackendConnectionManager::componentName() const
{
    return ip::backend_connection_manager_name;
}

}

// Local Variables: **
// mode: c++ **
// End: **
