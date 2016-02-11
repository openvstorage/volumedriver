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

#include <youtils/Assert.h>

namespace backend
{

namespace ip = initialized_params;

void
BackendConnectionDeleter::operator()(BackendConnectionInterface* conn)
{
    if (conn != nullptr)
    {
        cm_->releaseConnection(conn);
    }
}

// This thing makes it effectively singleton per backend!!
BackendConnectionManager::BackendConnectionManager(const boost::property_tree::ptree& pt,
                                                   const RegisterComponent registerize)
    : VolumeDriverComponent(registerize,
                            pt)
    , backend_connection_pool_capacity(pt)
    , backend_interface_retries_on_error(pt)
    , backend_interface_retry_interval_secs(pt)
    , config_(BackendConfig::makeBackendConfig(pt))
{
    switch (config_->backend_type.value())
    {
    case BackendType::LOCAL:
        {
            const LocalConfig* config(dynamic_cast<const LocalConfig*>(config_.get()));
            VERIFY(config);
            default_timeout = boost::posix_time::seconds(1);
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

void
BackendConnectionManager::releaseConnection(BackendConnectionInterface* conn)
{
#define LOCK_CONNS()                            \
    ::boost::lock_guard<lock_type> g__(lock_);

    LOG_TRACE("Returning connection handle " << conn);
    ASSERT(conn != nullptr);

    LOCK_CONNS();

    if (conn->healthy() and
        connections_.size() < backend_connection_pool_capacity.value())
    {
        connections_.push_front(conn);
    }
    else
    {
        delete conn;
    }
}

size_t
BackendConnectionManager::capacity() const
{
    LOCK_CONNS();
    return backend_connection_pool_capacity.value();
}

size_t
BackendConnectionManager::size() const
{
    LOCK_CONNS();
    return connections_.size();
}

BackendInterfacePtr
BackendConnectionManager::newBackendInterface(const Namespace& nspace)
{
    return BackendInterfacePtr(new BackendInterface(nspace,
                                                    shared_from_this(),
                                                    default_timeout,
                                                    backend_interface_retries_on_error.value(),
                                                    backend_interface_retry_interval_secs.value()));
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
    return std::unique_ptr<std::ostream>(new boost::iostreams::stream<ManagedBackendSink>(sink,
                                                                             bufsize));
}

std::unique_ptr<std::istream>
BackendConnectionManager::getInputStream(const Namespace& nspace,
                                         const std::string& name,
                                         size_t bufsize)
{
    ManagedBackendSource source(shared_from_this(), nspace, name);
    return std::unique_ptr<std::istream>(new boost::iostreams::stream<ManagedBackendSource>(source,
                                                                               bufsize));
}

void
BackendConnectionManager::persist(boost::property_tree::ptree& pt,
                                  const ReportDefault report_default) const
{
    config_->persist_internal(pt,
                              report_default);
    LOCK_CONNS();

#define P(x)                                            \
    x.persist(pt,                                       \
              report_default)

    P(backend_connection_pool_capacity);
    P(backend_interface_retries_on_error);
    P(backend_interface_retry_interval_secs);

#undef P
}

void
BackendConnectionManager::update(const boost::property_tree::ptree& pt,
                                 youtils::UpdateReport& report)
{
    config_->update_internal(pt, report);

    LOCK_CONNS();
    const decltype(backend_connection_pool_capacity) new_cap(pt);
    if (new_cap.value() < connections_.size())
    {
        int64_t diff = connections_.size() - new_cap.value();
        VERIFY(diff > 0);

        while (diff)
        {
            connections_.pop_back();
            --diff;
        }
    }

#define U(x)                                            \
    x.update(pt,                                        \
             report)

    U(backend_connection_pool_capacity);
    U(backend_interface_retries_on_error);
    U(backend_interface_retry_interval_secs);

#undef U
}

bool
BackendConnectionManager::checkConfig(const boost::property_tree::ptree& pt,
                                      youtils::ConfigurationReport& rep) const
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
