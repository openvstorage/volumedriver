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

#include "ShmContext.h"

namespace libvoldrv = libovsvolumedriver;

ShmContext::ShmContext()
    : shm_ctx_(nullptr)
{
}

ShmContext::~ShmContext()
{
}

int
ShmContext::open_volume(const char *volume_name, int oflag)
{
    try
    {
        shm_ctx_ = std::make_shared<ovs_shm_context>(volume_name, oflag);
        volname_ = std::string(volume_name);
        return 0;
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = EACCES;
    }
    catch (const ShmIdlInterface::VolumeNameAlreadyRegistered&)
    {
        errno = EBUSY;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

void
ShmContext::close_volume()
{
    shm_ctx_.reset();
}

int
ShmContext::create_volume(const char *volume_name,
                          uint64_t size)
{
    try
    {
        libvoldrv::ShmClient::create_volume(volume_name,
                                            size);
        return 0;
    }
    catch (const ShmIdlInterface::VolumeExists&)
    {
        errno = EEXIST;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

int
ShmContext::remove_volume(const char *volume_name)
{
    try
    {
        libvoldrv::ShmClient::remove_volume(volume_name);
        return 0;
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

int
ShmContext::truncate_volume(const char *volume_name,
                            uint64_t size)
{
    try
    {
        libvoldrv::ShmClient::truncate_volume(volume_name,
                                              size);
        return 0;
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

int
ShmContext::truncate(uint64_t size)
{
    return truncate_volume(volname_.c_str(), size);
}

int
ShmContext::snapshot_create(const char *volume_name,
                            const char *snapshot_name,
                            const uint64_t timeout)
{
    try
    {
        libvoldrv::ShmClient::create_snapshot(volume_name,
                                           snapshot_name,
                                           timeout);
        return 0;
    }
    catch (const ShmIdlInterface::PreviousSnapshotNotOnBackendException&)
    {
        errno = EBUSY;
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT;
    }
    catch (const ShmIdlInterface::SyncTimeoutException&)
    {
        errno = ETIMEDOUT;
    }
    catch (const ShmIdlInterface::SnapshotAlreadyExists&)
    {
        errno = EEXIST;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

int
ShmContext::snapshot_rollback(const char *volume_name,
                              const char *snapshot_name)
{
    try
    {
        libvoldrv::ShmClient::rollback_snapshot(volume_name,
                                                snapshot_name);
        return 0;
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT;
    }
    catch (const ShmIdlInterface::VolumeHasChildren&)
    {
        errno = ENOTEMPTY;
        //errno = ECHILD;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

int
ShmContext::snapshot_remove(const char *volume_name,
                            const char *snapshot_name)
{
    try
    {
        libvoldrv::ShmClient::delete_snapshot(volume_name,
                                              snapshot_name);
        return 0;
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT;
    }
    catch (const ShmIdlInterface::VolumeHasChildren&)
    {
        errno = ENOTEMPTY;
        //errno = ECHILD;
    }
    catch (const ShmIdlInterface::SnapshotNotFound&)
    {
        errno = ENOENT;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

void
ShmContext::list_snapshots(std::vector<std::string>& snaps,
                           const char *volume_name,
                           uint64_t *size,
                           int *saved_errno)
{
    try
    {
        snaps = libvoldrv::ShmClient::list_snapshots(volume_name,
                                                     size);
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        *saved_errno = ENOENT;
    }
    catch (...)
    {
        *saved_errno = EIO;
    }
}

int
ShmContext::is_snapshot_synced(const char *volume_name,
                               const char *snapshot_name)
{
    try
    {
        return libvoldrv::ShmClient::is_snapshot_synced(volume_name,
                                                        snapshot_name);
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT;
    }
    catch (const ShmIdlInterface::SnapshotNotFound&)
    {
        errno = ENOENT;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

int
ShmContext::list_volumes(std::vector<std::string>& volumes)
{
    try
    {
        volumes = libvoldrv::ShmClient::list_volumes();
        return 0;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

int
ShmContext::list_cluster_node_uri(std::vector<std::string>& /*uris*/)
{
    errno = ENOSYS;
    return -1;
}

int
ShmContext::send_read_request(struct ovs_aiocb *ovs_aiocbp,
                              ovs_aio_request *request)
{
    return shm_ctx_->shm_client_->send_read_request(ovs_aiocbp->aio_buf,
                                                    ovs_aiocbp->aio_nbytes,
                                                    ovs_aiocbp->aio_offset,
                                                    reinterpret_cast<void*>(request));
}

int
ShmContext::send_write_request(struct ovs_aiocb *ovs_aiocbp,
                               ovs_aio_request *request)
{
    return shm_ctx_->shm_client_->send_write_request(ovs_aiocbp->aio_buf,
                                                     ovs_aiocbp->aio_nbytes,
                                                     ovs_aiocbp->aio_offset,
                                                     reinterpret_cast<void*>(request));
}

int
ShmContext::send_flush_request(ovs_aio_request *request)
{
    struct ovs_aiocb *ovs_aiocbp = request->ovs_aiocbp;
    return shm_ctx_->shm_client_->send_write_request(ovs_aiocbp->aio_buf,
                                                     ovs_aiocbp->aio_nbytes,
                                                     ovs_aiocbp->aio_offset,
                                                     reinterpret_cast<void*>(request));
}

int
ShmContext::stat_volume(struct stat *st)
{
    try
    {
        return shm_ctx_->shm_client_->stat(volname_, st);
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

ovs_buffer_t*
ShmContext::allocate(size_t size)
{
    ovs_buffer_t *buf = shm_ctx_->cache_->allocate(size);
    if (!buf)
    {
        errno = ENOMEM;
    }
    return buf;
}

int
ShmContext::deallocate(ovs_buffer_t *ptr)
{
    int r = shm_ctx_->cache_->deallocate(ptr);
    if (r < 0)
    {
        errno = EFAULT;
    }
    return r;
}

bool
ShmContext::is_dtl_in_sync()
{
    std::abort();
    return false;
}
