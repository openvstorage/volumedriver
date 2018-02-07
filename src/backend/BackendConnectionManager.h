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
#include "ConnectionManagerParameters.h"
#include "Namespace.h"

#include <boost/chrono.hpp>
#include <boost/optional.hpp>

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
class RoundRobinPoolSelector;

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
           const RegisterComponent = RegisterComponent::T,
           const EnableConnectionHooks = EnableConnectionHooks::F);

    ~BackendConnectionManager() = default;

    BackendConnectionManager(const BackendConnectionManager&) = delete;

    BackendConnectionManager&
    operator=(const BackendConnectionManager&) = delete;

    BackendConnectionInterfacePtr
    getConnection(const ForceNewConnection force_new = ForceNewConnection::F,
                  const boost::optional<Namespace>& = boost::none);

    BackendInterfacePtr
    newBackendInterface(const Namespace&);

    const BackendConfig&
    config() const
    {
        return *config_;
    }

    const ConnectionManagerParameters&
    connection_manager_parameters() const
    {
        return params_;
    }

    size_t
    capacity() const;

    size_t
    size() const;

    using ConnectionPoolPtr = std::shared_ptr<ConnectionPool>;

    ConnectionPoolPtr
    pool(const Namespace&) const;

    using ConnectionPools = std::vector<ConnectionPoolPtr>;

    const ConnectionPools&
    pools() const
    {
        return connection_pools_;
    }

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
        return params_.backend_interface_retries_on_error.value();
    }

    boost::chrono::milliseconds
    retry_interval() const
    {
        return
            boost::chrono::milliseconds(params_.backend_interface_retry_interval_secs.value() * 1000);
    }

    double
    retry_backoff_multiplier() const
    {
        return params_.backend_interface_retry_backoff_multiplier.value();
    }

    bool
    partial_read_nullio() const
    {
        return params_.backend_interface_partial_read_nullio.value();
    }

    // REVISIT (pun intended): I don't like offering this - it might
    // be better to move the code that uses this (cf. BackendInterface)
    // into a method of this class?
    template<typename Visitor>
    void
    visit_pools(Visitor&& v)
    {
        for (auto p : connection_pools_)
        {
            v(p);
        }
    }

private:
    DECLARE_LOGGER("BackendConnectionManager");

    ConnectionManagerParameters params_;
    ConnectionPools connection_pools_;
    std::unique_ptr<BackendConfig> config_;
    std::atomic<uint64_t> next_pool_;

    explicit BackendConnectionManager(const boost::property_tree::ptree&,
                                      const RegisterComponent,
                                      const EnableConnectionHooks);

    friend class toolcut::BackendToolCut;
    friend class toolcut::BackendConnectionToolCut;
    friend class youtils::EnableMakeShared<BackendConnectionManager>;
    friend class RoundRobinPoolSelector;

    // TODO: I don't like this (and consequently the next_pool_ member) being here ...
    ConnectionPoolPtr&
    round_robin_select_()
    {
        const size_t n = connection_pools_.size();
        ASSERT(n > 0);
        return connection_pools_[next_pool_++ % n];
    }
};

}

#endif // !BACKEND_CONNECTION_MANAGER_H_

// Local Variables: **
// mode: c++ **
// End: **
