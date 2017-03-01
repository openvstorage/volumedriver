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
#include "NetworkXioContext.h"
#include "common_priv.h"

namespace libovsvolumedriver
{

NetworkXioContext::NetworkXioContext(const std::string& uri,
                                     uint64_t net_client_qdepth,
                                     NetworkHAContext& ha_ctx)
    : net_client_(nullptr)
    , uri_(uri)
    , net_client_qdepth_(net_client_qdepth)
    , ha_ctx_(ha_ctx)
{
    LIBLOGID_DEBUG("uri: " << uri <<
                   ",queue depth: " << net_client_qdepth);
}

NetworkXioContext::~NetworkXioContext()
{
}

int
NetworkXioContext::aio_suspend(ovs_aiocb *ovs_aiocbp)
{
    int r = 0;
    if (__sync_bool_compare_and_swap(&ovs_aiocbp->request_->_on_suspend,
                                     false,
                                     true,
                                     __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&ovs_aiocbp->request_->_mutex);
        while (not ovs_aiocbp->request_->_signaled && r == 0)
        {
            r = pthread_cond_wait(&ovs_aiocbp->request_->_cond,
                                  &ovs_aiocbp->request_->_mutex);
        }
        pthread_mutex_unlock(&ovs_aiocbp->request_->_mutex);
    }
    return r;
}

ssize_t
NetworkXioContext::aio_return(ovs_aiocb *ovs_aiocbp)
{
    int r;
    errno = ovs_aiocbp->request_->_errno;
    if (not ovs_aiocbp->request_->_failed)
    {
        r = ovs_aiocbp->request_->_rv;
    }
    else
    {
        r = -1;
    }
    return r;
}

ssize_t
NetworkXioContext::wait_aio_request(ovs_aiocb *ovs_aiocbp)
{
    ssize_t r = 0;
    if ((r = aio_suspend(ovs_aiocbp)) < 0)
    {
        return r;
    }
    return aio_return(ovs_aiocbp);
}

int
NetworkXioContext::open_volume(const char *volume_name,
                               int oflag)
{
    LIBLOGID_DEBUG("volume name: " << volume_name
                   << ",oflag: " << oflag);
    ssize_t r = 0;
    struct ovs_aiocb aio;

    volname_ = std::string(volume_name);

    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Open,
                                                    &aio,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }

    try
    {
        net_client_ = std::make_shared<NetworkXioClient>(uri_,
                                                         net_client_qdepth_,
                                                         ha_ctx_);
        net_client_->xio_send_open_request(volume_name, request.get());
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }

    if (r < 0)
    {
        return r;
    }
    return wait_aio_request(&aio);
}

void
NetworkXioContext::close_volume()
{
    net_client_.reset();
}

int
NetworkXioContext::create_volume(const char *volume_name,
                                 uint64_t size)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_create_volume(uri_,
                                            volume_name,
                                            size,
                                            request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
NetworkXioContext::truncate_volume(const char *volume_name,
                                   uint64_t offset)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_truncate_volume(uri_,
                                              volume_name,
                                              offset,
                                              request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
NetworkXioContext::truncate(uint64_t offset)
{
    return truncate_volume(volname_.c_str(), offset);
}

int
NetworkXioContext::remove_volume(const char *volume_name)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_remove_volume(uri_,
                                            volume_name,
                                            request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
NetworkXioContext::snapshot_create(const char *volume_name,
                                   const char *snapshot_name,
                                   const uint64_t timeout)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_create_snapshot(uri_,
                                              volume_name,
                                              snapshot_name,
                                              timeout,
                                              request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
NetworkXioContext::snapshot_rollback(const char *volume_name,
                                     const char *snapshot_name)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_rollback_snapshot(uri_,
                                                volume_name,
                                                snapshot_name,
                                                request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
NetworkXioContext::snapshot_remove(const char *volume_name,
                                   const char *snapshot_name)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_delete_snapshot(uri_,
                                              volume_name,
                                              snapshot_name,
                                              request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

void
NetworkXioContext::list_snapshots(std::vector<std::string>& snaps,
                                  const char *volume_name,
                                  uint64_t *size,
                                  int *saved_errno)
{
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        *saved_errno = ENOMEM;
        return;
    }
    try
    {
        NetworkXioClient::xio_list_snapshots(uri_,
                                             volume_name,
                                             snaps,
                                             size,
                                             request.get());
        *saved_errno = request->_errno;
    }
    catch (const std::bad_alloc&)
    {
        *saved_errno = ENOMEM;
    }
    catch (...)
    {
        *saved_errno = EIO;
    }
}

int
NetworkXioContext::is_snapshot_synced(const char *volume_name,
                                      const char *snapshot_name)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_is_snapshot_synced(uri_,
                                                 volume_name,
                                                 snapshot_name,
                                                 request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
NetworkXioContext::list_volumes(std::vector<std::string>& volumes)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_list_volumes(uri_,
                                           volumes,
                                           request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }
    return r;
}

int
NetworkXioContext::list_cluster_node_uri(std::vector<std::string>& uris)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_list_cluster_node_uri(uri_,
                                                    uris,
                                                    request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }
    return r;
}

int
NetworkXioContext::get_volume_uri(const char *volume_name,
                                  std::string& volume_uri)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_get_volume_uri(uri_,
                                             volume_name,
                                             volume_uri,
                                             request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }
    return r;
}

int
NetworkXioContext::get_cluster_multiplier(const char *volume_name,
                                          uint32_t *cluster_multiplier)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_get_cluster_multiplier(uri_,
                                                     volume_name,
                                                     cluster_multiplier,
                                                     request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }
    return r;
}

int
NetworkXioContext::get_clone_namespace_map(const char *volume_name,
                                           CloneNamespaceMap& cn)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_get_clone_namespace_map(uri_,
                                                      volume_name,
                                                      cn,
                                                      request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }
    return r;
}

int
NetworkXioContext::get_page(const char *volume_name,
                            const ClusterAddress ca,
                            ClusterLocationPage& cl)
{
    int r;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_get_page(uri_,
                                       volume_name,
                                       ca,
                                       cl,
                                       request.get());
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }
    return r;
}

int
NetworkXioContext::send_read_request(ovs_aio_request* request)
{
    int r = 0;
    ovs_aiocb *ovs_aiocbp = request->ovs_aiocbp;
    try
    {
        net_client_->xio_send_read_request(ovs_aiocbp->aio_buf,
                                           ovs_aiocbp->aio_nbytes,
                                           ovs_aiocbp->aio_offset,
                                           request);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
NetworkXioContext::send_write_request(ovs_aio_request *request)
{
    int r = 0;
    ovs_aiocb *ovs_aiocbp = request->ovs_aiocbp;
    try
    {
        net_client_->xio_send_write_request(ovs_aiocbp->aio_buf,
                                            ovs_aiocbp->aio_nbytes,
                                            ovs_aiocbp->aio_offset,
                                            request);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
NetworkXioContext::send_flush_request(ovs_aio_request* request)
{
    int r = 0;
    try
    {
        net_client_->xio_send_flush_request(request);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
NetworkXioContext::stat_volume(struct stat *st)
{
    int r;
    uint64_t size = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Noop,
                                                    nullptr,
                                                    nullptr);
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        NetworkXioClient::xio_stat_volume(uri_,
                                          volname_,
                                          &size,
                                          request.get());
        errno = request->_errno; r = request->_rv;
        if (r == 0)
        {
            /* 512 for now */
            st->st_blksize = 512;
            st->st_size = size;
        }
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

ovs_buffer_t*
NetworkXioContext::allocate(size_t size ATTR_UNUSED)
{
    errno = ENOSYS;
    return nullptr;
}

int
NetworkXioContext::deallocate(ovs_buffer_t *ptr ATTR_UNUSED)
{
    errno = ENOSYS;
    return -1;
}

bool
NetworkXioContext::is_dtl_in_sync()
{
    return net_client_->is_dtl_in_sync();
}

} //namespace libovsvolumedriver
