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

#include "BackendConfig.h"
#include "BackendConnectionManager.h"
#include "BackendInterface.h"
#include "BackendParameters.h"
#include "BackendSinkInterface.h"

#include "ManagedBackendSink.h"
#include "ManagedBackendSource.h"

#include "Local_Connection.h"
#include "Local_Sink.h"
#include "Local_Source.h"

#include "S3_Connection.h"
#include "Multi_Connection.h"
#include "Alba_Connection.h"

#include <boost/iostreams/stream.hpp>
#include <boost/thread/thread.hpp>

#include <youtils/Assert.h>

namespace backend
{

namespace bpt = boost::property_tree;
namespace bio = boost::iostreams;
namespace ip = initialized_params;
namespace yt = youtils;

void
BackendConnectionDeleter::operator()(BackendConnectionInterface* conn)
{
    if (conn != nullptr)
    {
        cm_->releaseConnection(conn);
    }
}

namespace
{
size_t
num_connection_pools()
{
    size_t n = boost::thread::hardware_concurrency();
    return n ? n : 1;
}

}

// This thing makes it effectively singleton per backend!!
BackendConnectionManager::BackendConnectionManager(const bpt::ptree& pt,
                                                   const RegisterComponent registerize)
    : VolumeDriverComponent(registerize,
                            pt)
    , connection_pools_(num_connection_pools())
    , backend_connection_pool_capacity(pt)
    , backend_interface_retries_on_error(pt)
    , backend_interface_retry_interval_secs(pt)
    , backend_interface_retry_backoff_multiplier(pt)
    , config_(BackendConfig::makeBackendConfig(pt))
{
    VERIFY(connection_pools_.size() > 0);

    switch (config_->backend_type.value())
    {
    case BackendType::LOCAL:
        {
            const LocalConfig* config(dynamic_cast<const LocalConfig*>(config_.get()));
            VERIFY(config);
            default_timeout_ = boost::posix_time::seconds(1);
            return;
        }
    case BackendType::S3:
        {
            const S3Config* config(dynamic_cast<const S3Config*>(config_.get()));
            VERIFY(config);
            return;
        }
    case BackendType::MULTI:
        {
            const MultiConfig* config(dynamic_cast<const MultiConfig*>(config_.get()));
            VERIFY(config);
            return;
        }
    case BackendType::ALBA:
        {
            const AlbaConfig* config(dynamic_cast<const AlbaConfig*>(config_.get()));
            VERIFY(config);
            return;

        }
    }

    UNREACHABLE
}

BackendConnectionManager::~BackendConnectionManager()
{
    for (auto& p : connection_pools_)
    {
        while (not p.connections_.empty())
        {
            std::unique_ptr<BackendConnectionInterface> conn(&p.connections_.front());
            p.connections_.pop_front();
        }
    }

    switch (config_->backend_type.value())
    {
    case BackendType::LOCAL:
        return;
    case BackendType::S3:
        return;
    case BackendType::MULTI:
        return;
    case BackendType::ALBA:
        return;
    }
    UNREACHABLE
}

BackendConnectionInterface*
BackendConnectionManager::newConnection_()
{
    switch (config_->backend_type.value())
    {
    case BackendType::LOCAL:
        {
            const LocalConfig* config(dynamic_cast<const LocalConfig*>(config_.get()));
            return new local::Connection(*config);
        }
    case BackendType::S3:
        {
            const S3Config* config(dynamic_cast<const S3Config*>(config_.get()));
            return new s3::Connection(*config);
        }
    case BackendType::MULTI:
        {
            const MultiConfig* config(dynamic_cast<const MultiConfig*>(config_.get()));
            return new multi::Connection(*config);
        }
    case BackendType::ALBA:
        {
            const AlbaConfig* config(dynamic_cast<const AlbaConfig*>(config_.get()));
            return new albaconn::Connection(*config);
        }
    }
    UNREACHABLE
}

size_t
BackendConnectionManager::capacity() const
{
    return backend_connection_pool_capacity.value();
}

size_t
BackendConnectionManager::size() const
{
    size_t n = 0;
    for (const auto& p : connection_pools_)
    {
        boost::lock_guard<decltype(p.lock_)> g(p.lock_);
        n += p.connections_.size();
    }

    return n;
}

BackendInterfacePtr
BackendConnectionManager::newBackendInterface(const Namespace& nspace)
{
    return BackendInterfacePtr(new BackendInterface(nspace,
                                                    shared_from_this()));
}

std::unique_ptr<BackendSinkInterface>
BackendConnectionManager::newBackendSink(const Namespace& nspace,
                                         const std::string& name)
{
    switch (config_->backend_type.value())
    {
    case BackendType::LOCAL:
        {
            BackendConnectionInterfacePtr bc = getConnection();
            std::unique_ptr<local::Connection>
                c(dynamic_cast<local::Connection*>(bc.get()));
            VERIFY(c.get() != 0);
            bc.release();
            return std::unique_ptr<BackendSinkInterface>(new local::Sink(std::move(c),
                                                                         nspace,
                                                                         name,
                                                                         boost::posix_time::seconds(0)));
        }
    case BackendType::S3:
        {
            LOG_FATAL("The S3 backend does not support output streaming");
            throw BackendNotImplementedException();
        }

    case BackendType::MULTI:
        {
            LOG_FATAL("The MULTI backend does not support output streaming");
            throw BackendNotImplementedException();
        }
    case BackendType::ALBA:
        {
            LOG_FATAL("The ALBA backend does not support output streaming");
            throw BackendNotImplementedException();
        }

    }
    UNREACHABLE
}

std::unique_ptr<BackendSourceInterface>
BackendConnectionManager::newBackendSource(const Namespace& nspace,
                                           const std::string& name)
{
    switch (config_->backend_type.value())
    {
    case BackendType::LOCAL:
        {
            BackendConnectionInterfacePtr bc = getConnection();
            std::unique_ptr<local::Connection>
                c(dynamic_cast<local::Connection*>(bc.get()));
            VERIFY(c.get() != 0);
            bc.release();
            return std::unique_ptr<BackendSourceInterface>(new local::Source(std::move(c),
                                                                             nspace,
                                                                             name,
                                                                             boost::posix_time::seconds(0)));
        }
    case BackendType::S3:
        LOG_FATAL("The S3 backend does not support input streaming");
        throw BackendFatalException();
        // return std::unique_ptr<BackendSourceInterface>();

    case BackendType::MULTI:
        LOG_FATAL("The MULTI backend does not support input streaming");
        throw BackendFatalException();

    case BackendType::ALBA:
        LOG_FATAL("The ALBA backend does not support input streaming");
        throw BackendFatalException();

    }

    UNREACHABLE
}

std::unique_ptr<std::ostream>
BackendConnectionManager::getOutputStream(const Namespace& nspace,
                                          const std::string& name,
                                          size_t bufsize)
{
    ManagedBackendSink sink(shared_from_this(), nspace, name);
    return std::unique_ptr<std::ostream>(new bio::stream<ManagedBackendSink>(sink,
                                                                             bufsize));
}

std::unique_ptr<std::istream>
BackendConnectionManager::getInputStream(const Namespace& nspace,
                                         const std::string& name,
                                         size_t bufsize)
{
    ManagedBackendSource source(shared_from_this(), nspace, name);
    return std::unique_ptr<std::istream>(new bio::stream<ManagedBackendSource>(source,
                                                                               bufsize));
}

void
BackendConnectionManager::persist(bpt::ptree& pt,
                                  const ReportDefault report_default) const
{
    config_->persist_internal(pt,
                              report_default);

#define P(x)                                    \
    x.persist(pt,                               \
              report_default)

    P(backend_connection_pool_capacity);
    P(backend_interface_retries_on_error);
    P(backend_interface_retry_interval_secs);
    P(backend_interface_retry_backoff_multiplier);

#undef P
}

void
BackendConnectionManager::update(const bpt::ptree& pt,
                                 yt::UpdateReport& report)
{
    config_->update_internal(pt, report);

    const decltype(backend_connection_pool_capacity) new_cap(pt);

    for (auto& p : connection_pools_)
    {
        boost::lock_guard<decltype(p.lock_)> g(p.lock_);
        while (p.connections_.size() > new_cap.value() / connection_pools_.size())
        {
            std::unique_ptr<BackendConnectionInterface> conn(&p.connections_.front());
            p.connections_.pop_front();
        }
    }

#define U(x)                                    \
    x.update(pt,                                \
             report)

    U(backend_connection_pool_capacity);
    U(backend_interface_retries_on_error);
    U(backend_interface_retry_interval_secs);
    U(backend_interface_retry_backoff_multiplier);

#undef U
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
