// Copyright (C) 2017 iNuron NV
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

#ifndef VD_DTL_CLIENT_INTERFACE_H_
#define VD_DTL_CLIENT_INTERFACE_H_

#include "../FailOverCacheConfig.h"
#include "../SCO.h"
#include "../SCOProcessorInterface.h"

#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/optional.hpp>
#include <boost/thread/future.hpp>

#include <youtils/AsioServiceManager.h>
#include <youtils/Logging.h>

namespace volumedriver
{

class ClusterLocation;
class FailOverCacheConfig;
class FailOverCacheEntry;
class OwnerTag;

namespace failovercache
{

class ClientInterface
{
public:
    using MaybeMilliSeconds = boost::optional<boost::chrono::milliseconds>;

protected:
    ClientInterface(const FailOverCacheConfig& config,
                    const backend::Namespace& nspace,
                    const LBASize lba_size,
                    const ClusterMultiplier cluster_mult,
                    const MaybeMilliSeconds& request_timeout,
                    const MaybeMilliSeconds& connect_timeout)
        : config_(config)
        , nspace_(nspace)
        , lba_size_(lba_size)
        , cluster_mult_(cluster_mult)
        , delete_failover_dir_(false)
        , request_timeout_(request_timeout)
        , connect_timeout_(connect_timeout)
    {}

public:
    virtual ~ClientInterface() = default;

    ClientInterface(const ClientInterface&) = delete;

    ClientInterface&
    operator=(const ClientInterface&) = delete;

    virtual boost::future<void>
    addEntries(std::vector<FailOverCacheEntry>) = 0;

    virtual boost::future<void>
    addEntries(const std::vector<ClusterLocation>&,
               uint64_t addr,
               const uint8_t*) = 0;

    // returns the SCO size - 0 indicates a problem.
    // Z42: throw instead!
    virtual uint64_t
    getSCOFromFailOver(SCO,
                       SCOProcessorFun) = 0;

    virtual void
    getSCORange(SCO& oldest,
                SCO& youngest) = 0;

    virtual void
    clear() = 0;

    virtual boost::future<void>
    flush() = 0;

    // Used to be 'throw ()' and was then converted to 'noexcept', but why do we
    // need this guarantee in the first place? I don't think callsites rely on
    // it?
    virtual void
    removeUpTo(const SCO) noexcept = 0;

    virtual void
    getEntries(SCOProcessorFun) = 0;

    virtual void
    setBusyLoopDuration(const boost::chrono::microseconds&)
    {
        LOG_WARN(nspace_ << ": setting busy loop duration is not supported");
    }

    virtual void
    setRequestTimeout(const MaybeMilliSeconds& t)
    {
        request_timeout_ = t;
    }

    MaybeMilliSeconds
    getRequestTimeout() const
    {
        return request_timeout_;
    }

    MaybeMilliSeconds
    getConnectTimeout() const
    {
        return connect_timeout_;
    }

    void
    delete_failover_dir()
    {
        LOG_INFO("Setting delete_failover_dir_ to true");
        delete_failover_dir_ = true;
    }

    LBASize
    lba_size() const
    {
        return lba_size_;
    }

    ClusterMultiplier
    cluster_multiplier() const
    {
        return cluster_mult_;
    }

    // move out to a ClientInterfaceFactory!?
    static std::unique_ptr<ClientInterface>
    create(boost::asio::io_service&,
           const youtils::ImplicitStrand,
           const FailOverCacheConfig&,
           const backend::Namespace&,
           const OwnerTag,
           const LBASize,
           const ClusterMultiplier,
           const MaybeMilliSeconds& request_timeout,
           const MaybeMilliSeconds& connect_timeout);

protected:
    const FailOverCacheConfig config_;
    const backend::Namespace nspace_;
    const LBASize lba_size_;
    const ClusterMultiplier cluster_mult_;
    bool delete_failover_dir_;
    boost::optional<boost::chrono::milliseconds> request_timeout_;
    boost::optional<boost::chrono::milliseconds> connect_timeout_;

private:
    DECLARE_LOGGER("DtlClientInterface");
};

}

}

#endif // !VD_DTL_CLIENT_INTERFACE_H_
