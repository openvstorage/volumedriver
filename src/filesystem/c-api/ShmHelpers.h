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

#ifndef SHM_HELPERS_H
#define SHM_HELPERS_H

static inline int
shm_create_context(ovs_ctx_t *ctx,
                   const char *volume_name,
                   int oflag)
{
    try
    {
        ctx->shm_ctx_ = new ovs_shm_context(volume_name,
                                            oflag);
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

static inline int
shm_create_volume(const char *volume_name,
                  uint64_t size)
{
    try
    {
        volumedriverfs::ShmClient::create_volume(volume_name, size);
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

static inline int
shm_remove_volume(const char *volume_name)
{
    try
    {
        volumedriverfs::ShmClient::remove_volume(volume_name);
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

static inline int
shm_snapshot_create(const char *volume_name,
                    const char *snapshot_name,
                    const uint64_t timeout)
{
    try
    {
        volumedriverfs::ShmClient::create_snapshot(volume_name,
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

static inline int
shm_snapshot_rollback(const char *volume_name,
                      const char *snapshot_name)
{
    try
    {
        volumedriverfs::ShmClient::rollback_snapshot(volume_name,
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

static inline int
shm_snapshot_remove(const char *volume_name,
                    const char *snapshot_name)
{
    try
    {
        volumedriverfs::ShmClient::delete_snapshot(volume_name,
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

static inline void
shm_list_snapshots(std::vector<std::string>& snaps,
                   const char *volume_name,
                   uint64_t *size,
                   int *saved_errno)
{
    try
    {
        snaps = volumedriverfs::ShmClient::list_snapshots(volume_name,
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

static inline int
shm_is_snapshot_synced(const char *volume_name,
                       const char *snapshot_name)
{
    try
    {
        return volumedriverfs::ShmClient::is_snapshot_synced(volume_name,
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

static inline int
shm_list_volumes(std::vector<std::string>& volumes)
{
    try
    {
        volumes = volumedriverfs::ShmClient::list_volumes();
        return 0;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

static inline int
shm_send_read_request(ovs_shm_context *shm_ctx_,
                      struct ovs_aiocb *ovs_aiocbp,
                      ovs_aio_request *request)
{
    return shm_ctx_->shm_client_->send_read_request(ovs_aiocbp->aio_buf,
                                                    ovs_aiocbp->aio_nbytes,
                                                    ovs_aiocbp->aio_offset,
                                                    reinterpret_cast<void*>(request));

}

static inline int
shm_send_write_request(ovs_shm_context *shm_ctx_,
                       struct ovs_aiocb *ovs_aiocbp,
                       ovs_aio_request *request)
{
    return shm_ctx_->shm_client_->send_write_request(ovs_aiocbp->aio_buf,
                                                     ovs_aiocbp->aio_nbytes,
                                                     ovs_aiocbp->aio_offset,
                                                     reinterpret_cast<void*>(request));

}

#endif //SHM_HELPER_H
