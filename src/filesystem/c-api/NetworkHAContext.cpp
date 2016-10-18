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

#include "Logger.h"
#include "NetworkHAContext.h"
#include "NetworkXioContext.h"
#include "IOThread.h"

#include <atomic>
#include <memory>
#include <thread>
#include <chrono>
#include <random>

#define HA_HANDLER_SLEEP_TIME   5ms

#define LOCK_INFLIGHT()                                 \
    std::lock_guard<std::mutex> ifrl_(inflight_reqs_lock_)

#define LOCK_SEEN()                                     \
    std::lock_guard<std::mutex> srl_(seen_reqs_lock_)

namespace volumedriverfs
{

MAKE_EXCEPTION(NetworkHAContextMemPoolException, fungi::IOException);

NetworkHAContext::NetworkHAContext(const std::string& uri,
                                   uint64_t net_client_qdepth,
                                   bool ha_enabled)
    : ctx_(std::shared_ptr<ovs_context_t>(
                new NetworkXioContext(uri,
                                      net_client_qdepth,
                                      *this,
                                      false)))
    , uri_(uri)
    , qd_(net_client_qdepth)
    , ha_enabled_(ha_enabled)
    , request_id_(0)
    , opened_(false)
    , openning_(true)
    , connection_error_(false)
{
    mpool = std::shared_ptr<xio_mempool>(
                    xio_mempool_create(-1,
                                       XIO_MEMPOOL_FLAG_REGULAR_PAGES_ALLOC),
                    xio_mempool_destroy);
    if (mpool == nullptr)
    {
        throw NetworkHAContextMemPoolException("failed to create memory pool");
    }

    (void) xio_mempool_add_slab(mpool.get(),
                                4096,
                                0,
                                qd_,
                                32,
                                0);
    (void) xio_mempool_add_slab(mpool.get(),
                                32768,
                                0,
                                32,
                                32,
                                0);
    (void) xio_mempool_add_slab(mpool.get(),
                                131072,
                                0,
                                8,
                                32,
                                0);

    if (ha_enabled)
    {
        try
        {
            ha_ctx_thread_.iothread_ =
                std::thread(&NetworkHAContext::ha_ctx_handler,
                            this,
                            reinterpret_cast<void*>(&ha_ctx_thread_));
        }
        catch (const std::system_error& e)
        {
            LIBLOG_ERROR("cannot create HA handler thread: " << e.what());
            throw;
        }
    }
}

NetworkHAContext::~NetworkHAContext()
{
    if (is_ha_enabled())
    {
        ha_ctx_thread_.stop();
        ha_ctx_thread_.reset_iothread();
    }
    ctx_.reset();
}

std::string
NetworkHAContext::get_rand_cluster_uri()
{
    static std::random_device rd;
    static std::mt19937 generator(rd());
    std::uniform_int_distribution<> dist(0,
                                         std::distance(cluster_nw_uris_.begin(),
                                                       cluster_nw_uris_.end())
                                         -1);
    auto it = cluster_nw_uris_.begin();
    std::advance(it, dist(generator));
    std::string uri = *it;
    cluster_nw_uris_.erase(it);
    return uri;
}

uint64_t
NetworkHAContext::assign_request_id(ovs_aio_request *request)
{
    request->_id = ++request_id_;
    return request->_id;
}

void
NetworkHAContext::insert_inflight_request(uint64_t id,
                                          ovs_aio_request *request)
{
    inflight_ha_reqs_.emplace(id, request);
}

void
NetworkHAContext::fail_inflight_requests(int errval)
{
    LOCK_INFLIGHT();
    for (auto req_it = inflight_ha_reqs_.cbegin();
            req_it != inflight_ha_reqs_.cend();)
    {
        ovs_aio_request::handle_xio_request_nosched(req_it->second,
                                                    -1,
                                                    errval);
        inflight_ha_reqs_.erase(req_it++);
    }
}

void
NetworkHAContext::insert_seen_request(uint64_t id)
{
    LOCK_SEEN();
    seen_ha_reqs_.push(id);
}

void
NetworkHAContext::remove_seen_requests()
{
    std::queue<uint64_t> q_;
    {
        LOCK_SEEN();
        seen_ha_reqs_.swap(q_);
    }
    while (not q_.empty())
    {
        uint64_t id = q_.front();
        q_.pop();
        {
            LOCK_INFLIGHT();
            inflight_ha_reqs_.erase(id);
        }
    }
}

void
NetworkHAContext::remove_all_seen_requests()
{
    LOCK_SEEN();
    while (not seen_ha_reqs_.empty())
    {
        uint64_t id = seen_ha_reqs_.front();
        seen_ha_reqs_.pop();
        LOCK_INFLIGHT();
        inflight_ha_reqs_.erase(id);
    }
}

void
NetworkHAContext::resend_inflight_requests()
{
    for (auto& req_entry: inflight_ha_reqs_)
    {
        auto request = req_entry.second;
        switch (request->_op)
        {
        case RequestOp::Read:
            atomic_get_ctx()->send_read_request(request->ovs_aiocbp,
                                                request);
            break;
        case RequestOp::Write:
            atomic_get_ctx()->send_write_request(request->ovs_aiocbp,
                                                 request);
            break;
        case RequestOp::Flush:
        case RequestOp::AsyncFlush:
            atomic_get_ctx()->send_flush_request(request);
            break;
        default:
            LIBLOG_ERROR("unknown inflight request, op:"
                         << static_cast<int>(request->_op));
        }
    }
}

int
NetworkHAContext::reconnect()
{
    int r = -1;
    bool connected = false;
    while (not cluster_nw_uris_.empty())
    {
        std::string uri = get_rand_cluster_uri();
        if (uri == uri_)
        {
            continue;
        }
        auto tmp_ctx = std::shared_ptr<NetworkXioContext>(
                new NetworkXioContext(uri,
                                      qd_,
                                      *this,
                                      true));
        r = tmp_ctx->open_volume_(volume_name_.c_str(),
                                  oflag_,
                                  false);
        if (r < 0)
        {
            LIBLOG_ERROR("reconnection to URI '" << uri << "' failed");
            tmp_ctx.reset();
            continue;
        }
        else
        {
            uri_ = uri;
            connected = true;
            atomic_xchg_ctx(std::dynamic_pointer_cast<ovs_context_t>(tmp_ctx));
        }
        if (connected)
        {
            update_cluster_node_uri();
            LIBLOG_INFO("reconnection to URI '" << uri << "' succeeded");
            break;
        }
    }
    return r;
}

void
NetworkHAContext::try_to_reconnect()
{
    LOCK_INFLIGHT();
    int r = reconnect();
    if (r < 0)
    {
        opened_ = false;
    }
    else
    {
        connection_error_ = false;
        resend_inflight_requests();
    }
}

void
NetworkHAContext::update_cluster_node_uri()
{
    /* cnanakos TODO: update on specific intervals? */
    int rl = list_cluster_node_uri(cluster_nw_uris_);
    if (rl < 0)
    {
        LIBLOG_ERROR("failed to update cluster node URI list: "
                     << strerror(errno));
    }
}

void
NetworkHAContext::ha_ctx_handler(void *arg)
{
    using namespace std::chrono_literals;
    IOThread *thread = reinterpret_cast<IOThread*>(arg);
    while (not thread->stopping)
    {
        if (is_connection_error())
        {
            remove_all_seen_requests();
            if (is_volume_open())
            {
                try_to_reconnect();
            }
            else
            {
                fail_inflight_requests(EIO);
            }
        }
        else
        {
            remove_seen_requests();
        }
        std::this_thread::sleep_for(HA_HANDLER_SLEEP_TIME);
    }
    std::lock_guard<std::mutex> lock_(thread->mutex_);
    thread->stopped = true;
    thread->cv_.notify_all();
}

int
NetworkHAContext::open_volume(const char *volume_name,
                              int oflag)
{
    int r = atomic_get_ctx()->open_volume(volume_name, oflag);
    if (not r)
    {
        volume_name_ = std::string(volume_name);
        oflag_ = oflag;
        openning_ = false;
        opened_ = true;
        update_cluster_node_uri();
    }
    return r;
}

void
NetworkHAContext::close_volume()
{
    atomic_get_ctx()->close_volume();
}

int
NetworkHAContext::create_volume(const char *volume_name,
                                uint64_t size)
{
    return atomic_get_ctx()->create_volume(volume_name, size);
}

int
NetworkHAContext::remove_volume(const char *volume_name)
{
    return atomic_get_ctx()->remove_volume(volume_name);
}

int
NetworkHAContext::truncate_volume(const char *volume_name,
                                  uint64_t size)
{
    return atomic_get_ctx()->truncate_volume(volume_name, size);
}

int
NetworkHAContext::truncate(uint64_t size)
{
    return atomic_get_ctx()->truncate(size);
}

int
NetworkHAContext::snapshot_create(const char *volume_name,
                                  const char *snapshot_name,
                                  const uint64_t timeout)
{
    return atomic_get_ctx()->snapshot_create(volume_name,
                                             snapshot_name,
                                             timeout);
}

int
NetworkHAContext::snapshot_rollback(const char *volume_name,
                                    const char *snapshot_name)
{
    return atomic_get_ctx()->snapshot_rollback(volume_name,
                                               snapshot_name);
}

int
NetworkHAContext::snapshot_remove(const char *volume_name,
                                  const char *snapshot_name)
{
    return atomic_get_ctx()->snapshot_remove(volume_name,
                                             snapshot_name);
}

void
NetworkHAContext::list_snapshots(std::vector<std::string>& snaps,
                                 const char *volume_name,
                                 uint64_t *size,
                                 int *saved_errno)
{
    atomic_get_ctx()->list_snapshots(snaps,
                                     volume_name,
                                     size,
                                     saved_errno);
}

int
NetworkHAContext::is_snapshot_synced(const char *volume_name,
                                     const char *snapshot_name)
{
    return atomic_get_ctx()->is_snapshot_synced(volume_name,
                                                snapshot_name);
}

int
NetworkHAContext::list_volumes(std::vector<std::string>& volumes)
{
    return atomic_get_ctx()->list_volumes(volumes);
}

int
NetworkHAContext::list_cluster_node_uri(std::vector<std::string>& uris)
{
    return atomic_get_ctx()->list_cluster_node_uri(uris);
}

int
NetworkHAContext::send_read_request(struct ovs_aiocb *ovs_aiocbp,
                                    ovs_aio_request *request)
{
    int r;
    if (is_ha_enabled())
    {
        uint64_t id = assign_request_id(request);
        LOCK_INFLIGHT();
        r = atomic_get_ctx()->send_read_request(ovs_aiocbp,
                                                request);
        if (not r)
        {
            insert_inflight_request(id, request);
        }
    }
    else
    {
        r = atomic_get_ctx()->send_read_request(ovs_aiocbp,
                                                request);
    }
    return r;
}

int
NetworkHAContext::send_write_request(struct ovs_aiocb *ovs_aiocbp,
                                     ovs_aio_request *request)
{
    int r;
    if (is_ha_enabled())
    {
        uint64_t id = assign_request_id(request);
        LOCK_INFLIGHT();
        r = atomic_get_ctx()->send_write_request(ovs_aiocbp,
                                                 request);
        if (not r)
        {
            insert_inflight_request(id, request);
        }
    }
    else
    {
        r = atomic_get_ctx()->send_write_request(ovs_aiocbp,
                                                 request);
    }
    return r;
}

int
NetworkHAContext::send_flush_request(ovs_aio_request *request)
{
    int r;
    if (is_ha_enabled())
    {
        uint64_t id = assign_request_id(request);
        LOCK_INFLIGHT();
        r = atomic_get_ctx()->send_flush_request(request);
        if (not r)
        {
            insert_inflight_request(id, request);
        }
    }
    else
    {
        r = atomic_get_ctx()->send_flush_request(request);
    }
    return r;
}

int
NetworkHAContext::stat_volume(struct stat *st)
{
    return atomic_get_ctx()->stat_volume(st);
}

ovs_buffer_t*
NetworkHAContext::allocate(size_t size)
{
    ovs_buffer_t *buf = new ovs_buffer_t;
    int r = xio_mempool_alloc(mpool.get(), size, &buf->mem);
    if (r < 0)
    {
        void *ptr;
        /* try to be in the safe side with 4k alignment */
        int ret = posix_memalign(&ptr, 4096, size);
        if (ret != 0)
        {
            errno = ret;
            delete buf;
            return NULL;
        }
        else
        {
            buf->buf = ptr;
            buf->size = size;
            buf->from_mpool = false;
        }
    }
    else
    {
        buf->buf = buf->mem.addr;
        buf->size = buf->mem.length;
        buf->from_mpool = true;
    }
    return buf;
}

int
NetworkHAContext::deallocate(ovs_buffer_t *ptr)
{
    if (ptr->from_mpool)
    {
        xio_mempool_free(&ptr->mem);
    }
    else
    {
        free(ptr->buf);
    }
    delete ptr;
    return 0;
}

} //namespace volumedriverfs
