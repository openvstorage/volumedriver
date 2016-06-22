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

#ifndef __SHM_CONTEXT_H
#define __SHM_CONTEXT_H

#include "ShmHandler.h"
#include "../ShmControlChannelProtocol.h"
#include "ShmClient.h"
#include "context.h"

struct ShmContext : public ovs_context_t
{
    std::shared_ptr<ovs_shm_context> shm_ctx_;
    ShmSegmentDetails segment_details_;

    explicit ShmContext(const ShmSegmentDetails& segment_details)
        : shm_ctx_(nullptr)
        , segment_details_(segment_details)
    {}

    ~ShmContext() = default;

    int
    open_volume(const char *volume_name,
                int oflag)
    {
        try
        {
            shm_ctx_ = std::make_shared<ovs_shm_context>(segment_details_,
                                                         volume_name,
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

    void
    close_volume()
    {
        shm_ctx_.reset();
    }

    int
    create_volume(const char *volume_name,
                  uint64_t size)
    {
        try
        {
            volumedriverfs::ShmOrbClient(segment_details_).create_volume(volume_name,
                                                                         size);
            return 0;
        }
        catch (const ShmIdlInterface::VolumeExists&)
        {
            errno = EEXIST;
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to create volume: " << e.what() << std::endl;
            errno = EIO;
        }
        catch (...)
        {
            errno = EIO;
        }
        return -1;
    }

    int
    remove_volume(const char *volume_name)
    {
        try
        {
            volumedriverfs::ShmOrbClient(segment_details_).remove_volume(volume_name);
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
    truncate_volume(const char *volume_name,
                    uint64_t size)
    {
        try
        {
            volumedriverfs::ShmOrbClient(segment_details_).truncate_volume(volume_name,
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
    truncate(uint64_t size)
    {
        try
        {
            shm_ctx_->shm_client_->truncate(size);
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
    snapshot_create(const char *volume_name,
                    const char *snapshot_name,
                    const uint64_t timeout)
    {
        try
        {
            volumedriverfs::ShmOrbClient(segment_details_).create_snapshot(volume_name,
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
    snapshot_rollback(const char *volume_name,
                      const char *snapshot_name)
    {
        try
        {
            volumedriverfs::ShmOrbClient(segment_details_).rollback_snapshot(volume_name,
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
    snapshot_remove(const char *volume_name,
                    const char *snapshot_name)
    {
        try
        {
            volumedriverfs::ShmOrbClient(segment_details_).delete_snapshot(volume_name,
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
    list_snapshots(std::vector<std::string>& snaps,
                   const char *volume_name,
                   uint64_t *size,
                   int *saved_errno)
    {
        try
        {
            snaps = volumedriverfs::ShmOrbClient(segment_details_).list_snapshots(volume_name,
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
    is_snapshot_synced(const char *volume_name,
                       const char *snapshot_name)
    {
        try
        {
            return volumedriverfs::ShmOrbClient(segment_details_).is_snapshot_synced(volume_name,
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
    list_volumes(std::vector<std::string>& volumes)
    {
        try
        {
            volumes = volumedriverfs::ShmOrbClient(segment_details_).list_volumes();
            return 0;
        }
        catch (...)
        {
            errno = EIO;
        }
        return -1;
    }

    int
    send_read_request(struct ovs_aiocb *ovs_aiocbp,
                      ovs_aio_request *request)
    {
        return shm_ctx_->shm_client_->send_read_request(ovs_aiocbp->aio_buf,
                                                        ovs_aiocbp->aio_nbytes,
                                                        ovs_aiocbp->aio_offset,
                                                        reinterpret_cast<void*>(request));
    }

    int
    send_write_request(struct ovs_aiocb *ovs_aiocbp,
                       ovs_aio_request *request)
    {
        return shm_ctx_->shm_client_->send_write_request(ovs_aiocbp->aio_buf,
                                                         ovs_aiocbp->aio_nbytes,
                                                         ovs_aiocbp->aio_offset,
                                                         reinterpret_cast<void*>(request));
    }

    int
    send_flush_request(ovs_aio_request *request)
    {
        struct ovs_aiocb *ovs_aiocbp = request->ovs_aiocbp;
        return shm_ctx_->shm_client_->send_write_request(ovs_aiocbp->aio_buf,
                                                         ovs_aiocbp->aio_nbytes,
                                                         ovs_aiocbp->aio_offset,
                                                         reinterpret_cast<void*>(request));
    }

    int
    stat_volume(struct stat *st)
    {
        try
        {
            assert(st);
            shm_ctx_->shm_client_->stat(*st);
            return 0;
        }
        catch (...)
        {
            errno = EIO;
        }
        return -1;
    }

    ovs_buffer_t*
    allocate(size_t size)
    {
        ovs_buffer_t *buf = shm_ctx_->cache_->allocate(size);
        if (!buf)
        {
            errno = ENOMEM;
        }
        return buf;
    }

    int
    deallocate(ovs_buffer_t *ptr)
    {
        int r = shm_ctx_->cache_->deallocate(ptr);
        if (r < 0)
        {
            errno = EFAULT;
        }
        return r;
    }
};

#endif //__SHM_CONTEXT_H
