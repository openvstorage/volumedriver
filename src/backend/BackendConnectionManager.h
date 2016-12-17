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

#ifndef BACKEND_CONNECTION_MANAGER_H_
#define BACKEND_CONNECTION_MANAGER_H_

#include "BackendConnectionInterface.h"
#include "BackendParameters.h"
#include "ConnectionPool.h"
#include "Namespace.h"

#include <boost/chrono.hpp>

#include <youtils/ConfigurationReport.h>
#include <youtils/EnableMakeShared.h>
#include <youtils/IOException.h>
#include <youtils/StrongTypedString.h>
#include <youtils/VolumeDriverComponent.h>

namespace youtils
{
class UpdateReport;
}

namespace toolcut
{

class BackendToolCut;
class BackendConnectionToolCut;

}

namespace backend
{

class BackendConnectionManager;
class BackendSinkInterface;
class BackendSourceInterface;
class BackendTestSetup;

using BackendConnectionManagerPtr = std::shared_ptr<BackendConnectionManager>;
using BackendInterfacePtr = std::unique_ptr<BackendInterface>;

class BackendConnectionManager
    : public youtils::VolumeDriverComponent
    , public std::enable_shared_from_this<BackendConnectionManager>
{
public:
    // enfore usage via BackendConnectionManagerPtr
    // rationale: BackendInterfaces hold a reference (via BackendConnectionManagerPtr),
    // so the BackendConnectionManager must not go out of scope before the last
    // reference is returned.
    static BackendConnectionManagerPtr
    create(const boost::property_tree::ptree&,
           const RegisterComponent = RegisterComponent::T);

    ~BackendConnectionManager() = default;

    BackendConnectionManager(const BackendConnectionManager&) = delete;

    BackendConnectionManager&
    operator=(const BackendConnectionManager&) = delete;

    BackendConnectionInterfacePtr
    getConnection(ForceNewConnection force_new = ForceNewConnection::F)
    {
        return get_connection_pool_().get_connection(force_new);
    }

    BackendInterfacePtr
    newBackendInterface(const Namespace&);

    const BackendConfig&
    config() const
    {
        return *config_;
    }

    std::unique_ptr<BackendSinkInterface>
    newBackendSink(const Namespace&,
                   const std::string& name);

    std::unique_ptr<std::ostream>
    getOutputStream(const Namespace&,
                    const std::string& name,
                    size_t buf_size = 4096);

    std::unique_ptr<BackendSourceInterface>
    newBackendSource(const Namespace&,
                     const std::string& name);

    std::unique_ptr<std::istream>
    getInputStream(const Namespace&,
                   const std::string& name,
                   size_t buf_size = 4096);

    size_t
    capacity() const;

    size_t
    size() const;

    size_t
    pools() const;

    // VolumeDriverComponent Interface
    virtual void
    persist(boost::property_tree::ptree&,
            const ReportDefault = ReportDefault::F) const override final;

    virtual const char*
    componentName() const override final;

    virtual void
    update(const boost::property_tree::ptree&,
           youtils::UpdateReport&) override final;

    virtual bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final;
    // end VolumeDriverComponent Interface

    uint32_t
    retries_on_error() const
    {
        return backend_interface_retries_on_error.value();
    }

    boost::chrono::milliseconds
    retry_interval() const
    {
        return
            boost::chrono::milliseconds(backend_interface_retry_interval_secs.value() * 1000);
    }

    double
    retry_backoff_multiplier() const
    {
        return backend_interface_retry_backoff_multiplier.value();
    }

    bool
    partial_read_nullio() const
    {
        return backend_interface_partial_read_nullio.value();
    }

private:
    DECLARE_LOGGER("BackendConnectionManager");

    DECLARE_PARAMETER(backend_connection_pool_capacity);
    DECLARE_PARAMETER(backend_connection_pool_shards);
    DECLARE_PARAMETER(backend_interface_retries_on_error);
    DECLARE_PARAMETER(backend_interface_retry_interval_secs);
    DECLARE_PARAMETER(backend_interface_retry_backoff_multiplier);
    DECLARE_PARAMETER(backend_interface_partial_read_nullio);

    // one per (logical) CPU.
    std::vector<std::shared_ptr<ConnectionPool>> connection_pools_;
    std::unique_ptr<BackendConfig> config_;

    explicit BackendConnectionManager(const boost::property_tree::ptree&,
                                      const RegisterComponent = RegisterComponent::T);

    ConnectionPool&
    get_connection_pool_()
    {
        int cpu = sched_getcpu();
        if (cpu < 0)
        {
            cpu = 0;
        }

        ASSERT(not connection_pools_.empty());
        const auto n = static_cast<unsigned>(cpu);

        return *connection_pools_[n % connection_pools_.size()];
    }

    friend class BackendTestSetup;
    friend class toolcut::BackendToolCut;
    friend class toolcut::BackendConnectionToolCut;
    friend class youtils::EnableMakeShared<BackendConnectionManager>;
};

}

#endif // !BACKEND_CONNECTION_MANAGER_H_

// Local Variables: **
// mode: c++ **
// End: **
