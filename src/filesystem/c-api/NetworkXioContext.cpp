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
    : uri_(uri)
    , net_client_qdepth_(net_client_qdepth)
    , ha_ctx_(ha_ctx)
    , net_client_(std::make_shared<NetworkXioClient>(uri_,
                                                     net_client_qdepth_,
                                                     ha_ctx_))
{
    LIBLOGID_DEBUG("uri: " << uri <<
                   ",queue depth: " << net_client_qdepth);
}

NetworkXioContext::~NetworkXioContext()
{
}

int
NetworkXioContext::aio_suspend(ovs_aio_request_ptr request)
{
    int r = 0;
    if (__sync_bool_compare_and_swap(&request->_on_suspend,
                                     false,
                                     true,
                                     __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&request->_mutex);
        while (not request->_signaled && r == 0)
        {
            r = pthread_cond_wait(&request->_cond,
                                  &request->_mutex);
        }
        pthread_mutex_unlock(&request->_mutex);
    }
    return r;
}

ssize_t
NetworkXioContext::aio_return(ovs_aio_request_ptr request)
{
    int r;
    errno = request->_errno;
    if (not request->_failed)
    {
        r = request->_rv;
    }
    else
    {
        r = -1;
    }
    return r;
}

ssize_t
NetworkXioContext::wait_aio_request(ovs_aio_request_ptr request)
{
    ssize_t r = 0;
    if ((r = aio_suspend(request)) < 0)
    {
        return r;
    }
    return aio_return(request);
}

int
NetworkXioContext::open_volume(const char *volume_name,
                               int oflag)
{
    LIBLOGID_DEBUG("volume name: " << volume_name
                   << ",oflag: " << oflag);
    int r = 0;

    volname_ = std::string(volume_name);

    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::Open,
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
    return wait_aio_request(request);
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
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::CreateVolume,
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
        net_client_->xio_create_volume(volume_name,
                                       size,
                                       request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::truncate_volume(const char *volume_name,
                                   uint64_t offset)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::TruncateVolume,
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
        net_client_->xio_truncate_volume(volume_name,
                                         offset,
                                         request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::truncate(uint64_t offset)
{
    return truncate_volume(volname_.c_str(), offset);
}

int
NetworkXioContext::remove_volume(const char *volume_name)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::RemoveVolume,
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
        net_client_->xio_remove_volume(volume_name,
                                       request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::snapshot_create(const char *volume_name,
                                   const char *snapshot_name,
                                   const uint64_t timeout)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::CreateSnapshot,
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
        net_client_->xio_create_snapshot(volume_name,
                                         snapshot_name,
                                         timeout,
                                         request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::snapshot_rollback(const char *volume_name,
                                     const char *snapshot_name)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::RollbackSnapshot,
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
        net_client_->xio_rollback_snapshot(volume_name,
                                           snapshot_name,
                                           request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::snapshot_remove(const char *volume_name,
                                   const char *snapshot_name)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::RemoveSnapshot,
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
        net_client_->xio_delete_snapshot(volume_name,
                                         snapshot_name,
                                         request.get());
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
    return wait_aio_request(request);
}

void
NetworkXioContext::list_snapshots(std::vector<std::string>& snaps,
                                  const char *volume_name,
                                  uint64_t *size,
                                  int *saved_errno)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::ListSnapshots,
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
        net_client_->xio_list_snapshots(volume_name,
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
    r = wait_aio_request(request);
    if (r < 0)
    {
        *saved_errno = errno;
    }
}

int
NetworkXioContext::is_snapshot_synced(const char *volume_name,
                                      const char *snapshot_name)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::IsSnapshotSynced,
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
        net_client_->xio_is_snapshot_synced(volume_name,
                                            snapshot_name,
                                            request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::list_volumes(std::vector<std::string>& volumes)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::ListVolumes,
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
        net_client_->xio_list_volumes(volumes,
                                      request.get());
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }

    if (r < 0)
    {
        return r;
    }
    return wait_aio_request(request);
}

int
NetworkXioContext::list_cluster_node_uri(std::vector<std::string>& uris)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::ListClusterNodeUri,
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
        net_client_->xio_list_cluster_node_uri(uris,
                                               request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::get_volume_uri(const char *volume_name,
                                  std::string& volume_uri)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::GetVolumeUri,
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
        net_client_->xio_get_volume_uri(volume_name,
                                        volume_uri,
                                        request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::get_cluster_multiplier(const char *volume_name,
                                          uint32_t *cluster_multiplier)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::GetClusterMultiplier,
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
        net_client_->xio_get_cluster_multiplier(volume_name,
                                                cluster_multiplier,
                                                request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::get_clone_namespace_map(const char *volume_name,
                                           CloneNamespaceMap& cn)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::GetCloneNamespaceMap,
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
        net_client_->xio_get_clone_namespace_map(volume_name,
                                                 cn,
                                                 request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::get_page(const char *volume_name,
                            const ClusterAddress ca,
                            ClusterLocationPage& cl)
{
    int r = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::GetPage,
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
        net_client_->xio_get_page(volume_name,
                                  ca,
                                  cl,
                                  request.get());
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
    return wait_aio_request(request);
}

int
NetworkXioContext::send_read_request(ovs_aio_request *request)
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
NetworkXioContext::send_flush_request(ovs_aio_request *request)
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
    int r = 0;
    uint64_t size = 0;
    std::shared_ptr<ovs_aio_request> request;
    try
    {
        request = std::make_shared<ovs_aio_request>(RequestOp::StatVolume,
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
        net_client_->xio_stat_volume(volname_,
                                     &size,
                                     request.get());
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    r = wait_aio_request(request);
    if (r == 0)
    {
        /* 512 for now */
        st->st_blksize = 512;
        st->st_size = size;
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
