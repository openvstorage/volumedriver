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

#ifndef FAILOVERCACHEPROXY_H
#define FAILOVERCACHEPROXY_H

#include "FailOverCacheCommand.h"
#include "SCOProcessorInterface.h"
#include "SCO.h"

#include "failovercache/ClientInterface.h"
#include "failovercache/fungilib/Socket.h"
#include "failovercache/fungilib/IOBaseStream.h"
#include "failovercache/fungilib/Buffer.h"

#include <vector>

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/scoped_array.hpp>
#include <boost/thread/future.hpp>

namespace volumedriver
{

class FailOverCacheConfig;
class FailOverCacheEntry;
class Volume;

class FailOverCacheProxy
    : public failovercache::ClientInterface
{
    friend class failovercache::ClientInterface;

    FailOverCacheProxy(const FailOverCacheConfig&,
                       const Namespace&,
                       const LBASize,
                       const ClusterMultiplier,
                       const failovercache::ClientInterface::MaybeMilliSeconds& request_timeout,
                       const failovercache::ClientInterface::MaybeMilliSeconds& connect_timeout);

public:
    FailOverCacheProxy(const FailOverCacheProxy&) = delete;

    FailOverCacheProxy&
    operator=(const FailOverCacheProxy&) = delete;

    virtual ~FailOverCacheProxy();

    boost::future<void>
    addEntries(std::vector<FailOverCacheEntry>) override final;

    boost::future<void>
    addEntries(const std::vector<ClusterLocation>&,
               uint64_t addr,
               const uint8_t*) override final;

    // returns the SCO size - 0 indicates a problem.
    // Z42: throw instead!
    uint64_t
    getSCOFromFailOver(SCO,
                       SCOProcessorFun) override final;

    void
    getSCORange(SCO& oldest,
                SCO& youngest) override final;

    void
    clear() override final;

    boost::future<void>
    flush() override final;

    void
    removeUpTo(const SCO) noexcept override final;

    void
    getEntries(SCOProcessorFun) override final;

    void
    setRequestTimeout(const failovercache::ClientInterface::MaybeMilliSeconds&) override final;

    void
    setBusyLoopDuration(const boost::chrono::microseconds&) override final;

private:
    DECLARE_LOGGER("FailOverCacheProxy");

    std::unique_ptr<fungi::Socket> socket_;
    fungi::IOBaseStream stream_;

    void
    register_();

    void
    unregister_();

    void
    check_(const char* desc);

    uint64_t
    getObject_(SCOProcessorFun,
               bool cork_per_cluster);

    template<FailOverCacheCommand,
             typename... Args>
    void
    send_(Args&&...);

    template<FailOverCacheCommand>
    void
    send_();
};

}

#endif // FAILOVERCACHEPROXY_H

// Local Variables: **
// mode: c++ **
// End: **
