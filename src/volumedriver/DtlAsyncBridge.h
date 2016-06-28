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

#ifndef VD_DTL_ASYNCBRIDGE_H_
#define VD_DTL_ASYNCBRIDGE_H_

#include "DtlBridgeCommon.h"
#include "DtlStreamers.h"
#include "DtlProxy.h"
#include "DtlClientInterface.h"
#include "SCO.h"

#include "distributed-transaction-log/fungilib/Runnable.h"
#include "distributed-transaction-log/fungilib/Thread.h"

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/FileDescriptor.h>
#include <youtils/IOException.h>

namespace volumedriver
{

class FailOverCacheTester;

class DtlAsyncBridge
    : public fungi::Runnable
    , public DtlClientInterface
{
    friend class FailOverCacheTester;

public:
    DtlAsyncBridge(const LBASize,
                             const ClusterMultiplier,
                             const size_t max_entries,
                             const std::atomic<unsigned>& write_trigger);

    DtlAsyncBridge(const DtlAsyncBridge&) = delete;

    ~DtlAsyncBridge() = default;

    DtlAsyncBridge&
    operator=(const DtlAsyncBridge&) = delete;

    virtual void
    initialize(DegradedFun) override;

    virtual void
    run() override;

    virtual const char*
    getName() const override;

    virtual void
    destroy(SyncFailOverToBackend) override;

    virtual bool
    addEntries(const std::vector<ClusterLocation>& locs,
               size_t num_locs,
               uint64_t start_address,
               const uint8_t* data) override;

    virtual bool
    backup() override;

    virtual void
    newCache(std::unique_ptr<DtlProxy> cache) override;

    virtual void
    setRequestTimeout(const boost::chrono::seconds) override;

    virtual void
    removeUpTo(const SCO& sconame) override;

    virtual uint64_t
    getSCOFromFailOver(SCO sconame,
                       SCOProcessorFun processor) override;

    virtual void
    Flush() override;

    virtual void
    Clear() override;

    virtual DtlMode
    mode() const override;

private:
    DECLARE_LOGGER("DtlAsyncBridge");

    void
    addEntry(ClusterLocation loc,
             uint64_t lba,
             const uint8_t* buf,
             size_t bufsize);

    void
    setThrottling(bool v);

    void
    maybe_swap_(bool new_sco);

    void
    init_cache_();

    void
    flush_();

    ClusterSize
    cluster_size_() const
    {
        return ClusterSize(cluster_multiplier_ * lba_size_);
    }

    std::unique_ptr<DtlProxy> cache_;

    // mutex_: protects stop_ and cache_
    // new_ones_mutex_: protects newOnes / oldOnes + buffers
    // lock order: mutex_ before new_ones_mutex_
    boost::mutex mutex_;
    boost::mutex new_ones_mutex_;
    boost::condition_variable condvar_;

    std::vector<DtlEntry> newOnes;
    std::vector<DtlEntry> oldOnes;
    const LBASize lba_size_;
    const ClusterMultiplier cluster_multiplier_;
    std::vector<uint8_t> newData;
    std::vector<uint8_t> oldData;
    const std::atomic<unsigned>& write_trigger_;

    // make configurable?
    static const boost::chrono::seconds timeout_;
    // number of entries before scheduling a write
    fungi::Thread* thread_;
    bool stop_;
    bool throttling;

    DegradedFun degraded_fun_;
};

}

#endif // VD_DTL_ASYNCBRIDGE_H_

// Local Variables: **
// mode: c++ **
// End: **
