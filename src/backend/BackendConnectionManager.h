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
#include "Namespace.h"

#include <boost/chrono.hpp>
#include <boost/thread/lock_guard.hpp>

#include <youtils/BooleanEnum.h>
#include <youtils/ConfigurationReport.h>
#include <youtils/IOException.h>
#include <youtils/SpinLock.h>
#include <youtils/StrongTypedString.h>
#include <youtils/VolumeDriverComponent.h>

VD_BOOLEAN_ENUM(ForceNewConnection)

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

class BackendConfig;
class BackendInterface;
typedef std::unique_ptr<BackendInterface> BackendInterfacePtr;

class BackendSinkInterface;
class BackendSourceInterface;

class BackendConnectionManager;
typedef std::shared_ptr<BackendConnectionManager> BackendConnectionManagerPtr;

class BackendConnectionDeleter
{
public:
    explicit BackendConnectionDeleter(BackendConnectionManagerPtr cm)
        : cm_(cm)
    {}

    ~BackendConnectionDeleter() = default;

    BackendConnectionDeleter(const BackendConnectionDeleter&) = default;

    BackendConnectionDeleter(BackendConnectionDeleter&&) = default;

    BackendConnectionDeleter&
    operator=(const BackendConnectionDeleter&) = default;

    BackendConnectionDeleter&
    operator=(BackendConnectionDeleter&&) = default;

    void
    operator()(BackendConnectionInterface* conn);

private:
    BackendConnectionManagerPtr cm_;
};

class BackendConnectionManager
    : public youtils::VolumeDriverComponent
    , public std::enable_shared_from_this<BackendConnectionManager>
{
private:
    // Uses boost::intrusive to avoid (de)allocations when modifying the container.
    // (which has to happen under the spinlock).
    struct ConnectionPool
    {
        mutable fungi::SpinLock lock_;
        boost::intrusive::slist<BackendConnectionInterface> connections_;
    };

public:
    // enfore usage via BackendConnectionManagerPtr
    // rationale: BackendConnPtrs hold a reference (via BackendConnectionManagerPtr),
    // so the BackendConnectionManager must not go out of scope before the last
    // reference is returned.
    static BackendConnectionManagerPtr
    create(const boost::property_tree::ptree& pt,
           const RegisterComponent registrate = RegisterComponent::T)
    {
        return BackendConnectionManagerPtr(new BackendConnectionManager(pt,
                                                                        registrate));
    }

    ~BackendConnectionManager();

    BackendConnectionManager(const BackendConnectionManager&) = delete;

    BackendConnectionManager&
    operator=(const BackendConnectionManager&) = delete;

    inline BackendConnectionInterfacePtr
    getConnection(ForceNewConnection force_new = ForceNewConnection::F)
    {
        ConnectionPool& pool = get_connection_pool_();
        BackendConnectionDeleter d(shared_from_this());
        BackendConnectionInterfacePtr conn(nullptr, d);

        {
            boost::lock_guard<decltype(pool.lock_)> g(pool.lock_);
            if (force_new != ForceNewConnection::T and not pool.connections_.empty())
            {
                conn = BackendConnectionInterfacePtr(&pool.connections_.front(),
                                                     d);
                pool.connections_.pop_front();
            }
        }

        if (not conn)
        {
            conn = BackendConnectionInterfacePtr(newConnection_(),
                                                 d);
        }

        conn->timeout(default_timeout_);
        return conn;
    }

    BackendInterfacePtr
    newBackendInterface(const Namespace&);

    const BackendConfig&
    config() const
    {
        return *config_;
    }

    std::unique_ptr<BackendSinkInterface>
    newBackendSink(const Namespace& nspace,
                   const std::string& name);

    std::unique_ptr<std::ostream>
    getOutputStream(const Namespace& nspace,
                    const std::string& name,
                    size_t buf_size = 4096);

    std::unique_ptr<BackendSourceInterface>
    newBackendSource(const Namespace& nspace,
                     const std::string& name);

    std::unique_ptr<std::istream>
    getInputStream(const Namespace& nspace,
                   const std::string& name,
                   size_t buf_size = 4096);

    size_t
    capacity() const;

    size_t
    size() const;

    // VolumeDriverComponent Interface
    virtual void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault reportDefault = ReportDefault::F) const override final;

    virtual const char*
    componentName() const override final;

    virtual void
    update(const boost::property_tree::ptree& pt,
           youtils::UpdateReport& report) override final;

    virtual bool
    checkConfig(const boost::property_tree::ptree& pt,
                youtils::ConfigurationReport& rep) const override final;
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

    boost::posix_time::time_duration
    default_timeout() const
    {
        return default_timeout_;
    }

private:
    DECLARE_LOGGER("BackendConnectionManager");

    // one per (logical) CPU.
    std::vector<ConnectionPool> connection_pools_;

    DECLARE_PARAMETER(backend_connection_pool_capacity);
    DECLARE_PARAMETER(backend_interface_retries_on_error);
    DECLARE_PARAMETER(backend_interface_retry_interval_secs);
    DECLARE_PARAMETER(backend_interface_retry_backoff_multiplier);

    std::unique_ptr<BackendConfig> config_;

    // Create through
    // static BackendConnectionManagerPtr
    // create(const boost::property_tree::ptree& pt,
    //        const RegisterComponent registrate = RegisterComponent::T)
    explicit BackendConnectionManager(const boost::property_tree::ptree&,
                                      const RegisterComponent reg = RegisterComponent::T);

    BackendConnectionInterface*
    newConnection_();

    boost::posix_time::time_duration default_timeout_;

    void
    releaseConnection(BackendConnectionInterface* conn)
    {
        ASSERT(conn != nullptr);
        if (conn->healthy())
        {
            ConnectionPool& pool = get_connection_pool_();
            boost::lock_guard<decltype(pool.lock_)> g(pool.lock_);
            if (pool.connections_.size() < backend_connection_pool_capacity.value() / connection_pools_.size())
            {
                pool.connections_.push_front(*conn);
                return;
            }
        }

        delete conn;
    }

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
        ASSERT(n < connection_pools_.size());

        return connection_pools_[n % connection_pools_.size()];
    }

    friend class BackendConnectionDeleter;
    friend class toolcut::BackendToolCut;
    friend class toolcut::BackendConnectionToolCut;
};

}

#endif // !BACKEND_CONNECTION_MANAGER_H_

// Local Variables: **
// mode: c++ **
// End: **
