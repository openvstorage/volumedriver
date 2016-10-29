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

#ifndef LIB_OVS_VOLUMEDRIVER_PATH_SELECTOR_H_
#define LIB_OVS_VOLUMEDRIVER_PATH_SELECTOR_H_

#include "context.h"
#include "RequestDispatcherCallback.h"

#include <memory>
#include <mutex>
#include <unordered_map>

#include <youtils/PeriodicAction.h>
#include <youtils/SpinLock.h>

namespace libovsvolumedriver
{

class PathSelector
    : public ovs_context_t
    , public RequestDispatcherCallback
{
public:
    using ContextPtr = std::shared_ptr<ovs_context_t>;
    using ContextFactoryFun = std::function<ContextPtr(const std::string& uri,
                                                       RequestDispatcherCallback&)>;

    PathSelector(const std::string& uri,
                 ContextFactoryFun,
                 RequestDispatcherCallback&);

    ~PathSelector() = default;

    PathSelector(const PathSelector&) = delete;

    PathSelector&
    operator=(const PathSelector&) = delete;

    int
    open_volume(const char *volume_name,
                int oflag) override final
    {
        return get_ctx_()->open_volume(volume_name, oflag);
    }

    void
    close_volume() override final
    {
        return get_ctx_()->close_volume();
    }

    int
    create_volume(const char *volume_name,
                  uint64_t size) override final
    {
        return get_ctx_()->create_volume(volume_name, size);
    }

    int
    remove_volume(const char *volume_name) override final
    {
        return get_ctx_()->remove_volume(volume_name);
    }

    int
    snapshot_create(const char *volume_name,
                    const char *snapshot_name,
                    const uint64_t timeout) override final
    {
        return get_ctx_()->snapshot_create(volume_name, snapshot_name, timeout);
    }

    int
    snapshot_rollback(const char *volume_name,
                      const char *snapshot_name) override final
    {
        return get_ctx_()->snapshot_rollback(volume_name, snapshot_name);
    }

    int
    snapshot_remove(const char *volume_name,
                    const char *snapshot_name) override final
    {
        return get_ctx_()->snapshot_remove(volume_name, snapshot_name);
    }
    void
    list_snapshots(std::vector<std::string>& snaps,
                   const char *volume_name,
                   uint64_t *size,
                   int *saved_errno) override final
    {
        return get_ctx_()->list_snapshots(snaps, volume_name, size, saved_errno);
    }

    int
    is_snapshot_synced(const char *volume_name,
                       const char *snapshot_name) override final
    {
        return get_ctx_()->is_snapshot_synced(volume_name, snapshot_name);
    }

    int
    list_volumes(std::vector<std::string>& vols) override final
    {
        return get_ctx_()->list_volumes(vols);
    }

    int
    list_cluster_node_uri(std::vector<std::string>& uris) override final
    {
        return get_ctx_()->list_cluster_node_uri(uris);
    }

    int
    get_volume_uri(const char* volume_name,
                   std::string& uri) override final
    {
        return get_ctx_()->get_volume_uri(volume_name, uri);
    }

    int
    send_read_request(ovs_aio_request*,
                      ovs_aiocb*) override final;

    int
    send_write_request(ovs_aio_request*,
                       ovs_aiocb*) override final;

    int
    send_flush_request(ovs_aio_request*) override final;

    int
    stat_volume(struct stat *st) override final
    {
        return get_ctx_()->stat_volume(st);
    }

    ovs_buffer*
    allocate(size_t size) override final
    {
        return get_ctx_()->allocate(size);
    }

    int
    deallocate(ovs_buffer* buf) override final
    {
        return get_ctx_()->deallocate(buf);
    }

    int
    truncate_volume(const char *volume_name,
                    uint64_t size) override final
    {
        return get_ctx_()->truncate_volume(volume_name, size);
    }

    int
    truncate(uint64_t size) override final
    {
        return get_ctx_()->truncate(size);
    }

    std::string
    current_uri() const override final
    {
        return get_ctx_()->current_uri();
    }

    boost::optional<std::string>
    volume_name() const override final
    {
        return get_ctx_()->volume_name();
    }

    void
    complete_request(ovs_aio_request&,
                     ssize_t ret,
                     int err,
                     bool schedule) override final;

private:
    ContextPtr ctx_;
    mutable fungi::SpinLock ctx_lock_;

    // consider using a custom - boost::pool based? - allocator,
    // especially since the map is protected by a spinlock.
    std::unordered_map<uint64_t, ContextPtr> map_;
    mutable fungi::SpinLock map_lock_;

    ContextFactoryFun make_context_;
    RequestDispatcherCallback& callback_;
    std::atomic<uint64_t> period_secs_;
    youtils::PeriodicAction checker_;

    ContextPtr
    get_ctx_() const
    {
        std::lock_guard<decltype(ctx_lock_)> g(ctx_lock_);
        return ctx_;
    }

    template<typename... Args>
    int
    wrap_io_(int (ovs_context_t::*mem_fun)(ovs_aio_request*,
                                           Args...),
             ovs_aio_request*,
             Args...);

    void
    update_();
};

}

#endif // !LIB_OVS_VOLUMEDRIVER_PATH_SELECTOR_H_
