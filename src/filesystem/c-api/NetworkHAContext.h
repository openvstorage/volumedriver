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

#ifndef __NETWORK_HA_CONTEXT_H
#define __NETWORK_HA_CONTEXT_H

#include "volumedriver.h"
#include "common.h"
#include "context.h"
#include "IOThread.h"

#include <youtils/SpinLock.h>

#include <libxio.h>

#include <atomic>
#include <memory>
#include <queue>
#include <map>
#include <unordered_map>

namespace libovsvolumedriver
{

class NetworkHAContext : public ovs_context_t
{
public:
    NetworkHAContext(const std::string& uri,
                     uint64_t net_client_qdepth,
                     bool ha_enabled);

    ~NetworkHAContext();

    int
    open_volume(const char *volume_name,
                int oflag);

    void
    close_volume();

    int
    create_volume(const char *volume_name,
                  uint64_t size);

    int
    remove_volume(const char *volume_name);

    int
    truncate_volume(const char *volume_name,
                    uint64_t size);

    int
    truncate(uint64_t size);

    int
    snapshot_create(const char *volume_name,
                    const char *snapshot_name,
                    const uint64_t timeout);

    int
    snapshot_rollback(const char *volume_name,
                      const char *snapshot_name);

    int
    snapshot_remove(const char *volume_name,
                    const char *snapshot_name);

    void
    list_snapshots(std::vector<std::string>& snaps,
                   const char *volume_name,
                   uint64_t *size,
                   int *saved_errno);

    int
    is_snapshot_synced(const char *volume_name,
                       const char *snapshot_name);

    int
    list_volumes(std::vector<std::string>& volumes);

    int
    list_cluster_node_uri(std::vector<std::string>& uris);

    int
    get_volume_uri(const char* volume_name,
                   std::string& volume_uri) override final;

    int
    send_read_request(ovs_aio_request*,
                      ovs_aiocb*) override final;

    int
    send_write_request(ovs_aio_request*,
                       ovs_aiocb*) override final;

    int
    send_flush_request(ovs_aio_request*) override final;

    int
    stat_volume(struct stat *st);

    ovs_buffer_t*
    allocate(size_t size);

    int
    deallocate(ovs_buffer_t *ptr);

    void
    insert_seen_request(uint64_t id);

    bool
    is_ha_enabled() const
    {
        return ha_enabled_;
    }

    bool
    is_volume_openning() const
    {
        return openning_;
    }

    bool
    is_volume_open() const
    {
        return opened_;
    }

    void
    set_connection_error()
    {
        if (is_ha_enabled())
        {
            connection_error_ = true;
        }
    }

private:
    /* cnanakos TODO: use atomic overloads for shared_ptr
     * when g++ > 5.0.0 is used
     */
    using ContextPtr = std::shared_ptr<ovs_context_t>;
    ContextPtr ctx_;

    fungi::SpinLock ctx_lock_;
    std::string volume_name_;
    int oflag_;
    std::string uri_;
    uint64_t qd_;
    bool ha_enabled_;
    std::shared_ptr<xio_mempool> mpool;

    std::mutex inflight_reqs_lock_;

    // Cling to a context as long as it's still having outstanding requests.
    // Look into a custom allocator for the map (e.g. based on boost::pool?) if
    // allocations get in the way.
    using RequestAndContext = std::pair<ovs_aio_request*, ContextPtr>;
    std::unordered_map<uint64_t, RequestAndContext> inflight_ha_reqs_;

    std::mutex seen_reqs_lock_;
    std::queue<uint64_t> seen_ha_reqs_;

    std::atomic<uint64_t> request_id_;

    std::vector<std::string> cluster_nw_uris_;

    IOThread ha_ctx_thread_;

    bool opened_;
    bool openning_;
    bool connection_error_;

    ContextPtr
    atomic_get_ctx()
    {
        fungi::ScopedSpinLock l_(ctx_lock_);
        return ctx_;
    }

    void
    atomic_xchg_ctx(ContextPtr ctx)
    {
        fungi::ScopedSpinLock l_(ctx_lock_);
        ctx_.swap(ctx);
    }

    // TODO:
    // * these need to become private - fix the callsite in NetworkXioRequest::open
    // * move assign_request_id to the ctor of ovs_aio_request
public:
    uint64_t
    assign_request_id(ovs_aio_request*);

    void
    insert_inflight_request(ovs_aio_request*,
                            ContextPtr);

private:
    template<typename... Args>
    int
    wrap_io(int (ovs_context_t::*mem_fun)(ovs_aio_request*, Args...),
            ovs_aio_request*,
            Args...);

    bool
    is_connection_error() const
    {
        return connection_error_;
    }

    void
    update_cluster_node_uri();

    void
    ha_ctx_handler(void *arg);

    void
    fail_inflight_requests(int errval);

    void
    resend_inflight_requests();

    void
    remove_seen_requests();

    void
    remove_all_seen_requests();

    void
    try_to_reconnect();

    int
    reconnect();

    std::string
    get_rand_cluster_uri();
};

} //namespace libovsvolumedriver

#endif //__NETWORK_HA_CONTEXT_H
