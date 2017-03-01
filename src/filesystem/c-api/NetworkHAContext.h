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
#include "common_priv.h"
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

class NetworkXioContext;
typedef std::shared_ptr<NetworkXioContext> NetworkXioContextPtr;

class NetworkHAContext : public ovs_context_t
{
public:
    NetworkHAContext(const std::string& uri,
                     uint64_t net_client_qdepth,
                     bool ha_enabled);

    ~NetworkHAContext();

    int
    open_volume(const char *volume_name,
                int oflag) override final;

    void
    close_volume() override final;

    int
    create_volume(const char *volume_name,
                  uint64_t size) override final;

    int
    remove_volume(const char *volume_name) override final;

    int
    truncate_volume(const char *volume_name,
                    uint64_t size) override final;

    int
    truncate(uint64_t size) override final;

    int
    snapshot_create(const char *volume_name,
                    const char *snapshot_name,
                    const uint64_t timeout) override final;

    int
    snapshot_rollback(const char *volume_name,
                      const char *snapshot_name) override final;

    int
    snapshot_remove(const char *volume_name,
                    const char *snapshot_name) override final;

    void
    list_snapshots(std::vector<std::string>& snaps,
                   const char *volume_name,
                   uint64_t *size,
                   int *saved_errno) override final;

    int
    is_snapshot_synced(const char *volume_name,
                       const char *snapshot_name) override final;

    int
    list_volumes(std::vector<std::string>& volumes) override final;

    int
    list_cluster_node_uri(std::vector<std::string>& uris) override final;

    int
    get_volume_uri(const char* volume_name,
                   std::string& volume_uri) override final;

    int
    get_cluster_multiplier(const char *volume_name,
                           uint32_t *cluster_multiplier) override final;

    int
    get_clone_namespace_map(const char *volume_name,
                            CloneNamespaceMap& cn) override final;

    int
    get_page(const char *volume_name,
             const ClusterAddress ca,
             ClusterLocationPage& cl) override final;

    int
    send_read_request(ovs_aio_request*) override final;

    int
    send_write_request(ovs_aio_request*) override final;

    int
    send_flush_request(ovs_aio_request*) override final;

    int
    stat_volume(struct stat *st) override final;

    ovs_buffer_t*
    allocate(size_t size) override final;

    int
    deallocate(ovs_buffer_t *ptr) override final;

    bool
    is_dtl_in_sync() override final;

    void
    insert_seen_request(uint64_t id);

    std::string
    current_uri() const;

    std::string
    volume_name() const;

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
    fungi::SpinLock ctx_lock_;
    NetworkXioContextPtr ctx_;

    // protects volume_name_ and uri_
    mutable std::mutex config_lock_;
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
    using RequestAndContext = std::pair<ovs_aio_request*,
                                        NetworkXioContextPtr>;
    std::unordered_map<uint64_t, RequestAndContext> inflight_ha_reqs_;

    std::mutex seen_reqs_lock_;
    std::queue<uint64_t> seen_ha_reqs_;

    std::atomic<uint64_t> request_id_;

    // Order: preferred/nearest ones at the end
    std::vector<std::string> cluster_nw_uris_;

    IOThread ha_ctx_thread_;

    bool opened_;
    bool openning_;
    bool connection_error_;

    bool
    is_connection_error() const
    {
        return connection_error_;
    }

    NetworkXioContextPtr
    atomic_get_ctx()
    {
        fungi::ScopedSpinLock l_(ctx_lock_);
        return ctx_;
    }

    void
    atomic_xchg_ctx(NetworkXioContextPtr ctx)
    {
        fungi::ScopedSpinLock l_(ctx_lock_);
        ctx_.swap(ctx);
    }

    template<typename... Args>
    int
    wrap_io(int (NetworkXioContext::*mem_fun)(ovs_aio_request*, Args...),
            ovs_aio_request*,
            Args...);

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

    int
    do_reconnect(const std::string& uri);

    std::string
    get_next_cluster_uri();

    void
    maybe_update_volume_location();

    uint64_t
    assign_request_id(ovs_aio_request*);

    void
    insert_inflight_request(uint64_t id,
                            ovs_aio_request*,
                            NetworkXioContextPtr);

    void
    current_uri(const std::string& u);

    void
    volume_name(const std::string& n);
};

} //namespace libovsvolumedriver

#endif //__NETWORK_HA_CONTEXT_H
