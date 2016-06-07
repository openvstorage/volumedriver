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

#ifndef __NETWORK_XIO_CONTEXT_H
#define __NETWORK_XIO_CONTEXT_H

#include "NetworkXioHandler.h"
#include "context.h"

struct NetworkXioContext : public ovs_context_t
{
    volumedriverfs::NetworkXioClientPtr net_client_;
    std::string uri_;
    uint64_t net_client_qdepth_;
    std::string volname_;

    NetworkXioContext(const std::string& uri,
                      uint64_t net_client_qdepth)
    : net_client_(nullptr)
    , uri_(uri)
    , net_client_qdepth_(net_client_qdepth) {}

    ~NetworkXioContext() {}

    int
    aio_suspend(ovs_aiocb *ovs_aiocbp)
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
    aio_return(ovs_aiocb *ovs_aiocbp)
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

    int
    open_volume(const char *volume_name,
                int oflag __attribute__((unused)))
    {
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
            net_client_ =
                std::make_shared<volumedriverfs::NetworkXioClient>(uri_,
                        net_client_qdepth_);
            net_client_->xio_send_open_request(volume_name,
                                               reinterpret_cast<void*>(request.get()));
        }
        catch (const volumedriverfs::XioClientQueueIsBusyException&)
        {
            errno = EBUSY;  r = -1;
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

        if ((r = aio_suspend(&aio)) < 0)
        {
            return r;
        }
        return aio_return(&aio);
    }

    void
    close_volume()
    {
        net_client_.reset();
    }

    int
    create_volume(const char *volume_name,
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
            volumedriverfs::NetworkXioClient::xio_create_volume(uri_,
                                                                volume_name,
                                                                size,
                                                                static_cast<void*>(request.get()));
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
    remove_volume(const char *volume_name)
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
            volumedriverfs::NetworkXioClient::xio_remove_volume(uri_,
                                                                volume_name,
                                                                static_cast<void*>(request.get()));
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
    snapshot_create(const char *volume_name,
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
            volumedriverfs::NetworkXioClient::xio_create_snapshot(uri_,
                                                                  volume_name,
                                                                  snapshot_name,
                                                                  timeout,
                                                                  static_cast<void*>(request.get()));
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
    snapshot_rollback(const char *volume_name,
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
            volumedriverfs::NetworkXioClient::xio_rollback_snapshot(uri_,
                                                                    volume_name,
                                                                    snapshot_name,
                                                                    static_cast<void*>(request.get()));
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
    snapshot_remove(const char *volume_name,
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
            volumedriverfs::NetworkXioClient::xio_delete_snapshot(uri_,
                                                                  volume_name,
                                                                  snapshot_name,
                                                                  static_cast<void*>(request.get()));
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
    list_snapshots(std::vector<std::string>& snaps,
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
            volumedriverfs::NetworkXioClient::xio_list_snapshots(uri_,
                                                                 volume_name,
                                                                 snaps,
                                                                 size,
                                                                 static_cast<void*>(request.get()));
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
    is_snapshot_synced(const char *volume_name,
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
            volumedriverfs::NetworkXioClient::xio_is_snapshot_synced(uri_,
                                                                     volume_name,
                                                                     snapshot_name,
                                                                     static_cast<void*>(request.get()));
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
    list_volumes(std::vector<std::string>& volumes)
    {
        try
        {
            volumedriverfs::NetworkXioClient::xio_list_volumes(uri_,
                                                               volumes);
            return 0;
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

    int
    send_read_request(struct ovs_aiocb *ovs_aiocbp,
                      ovs_aio_request *request)
    {
        int r = 0;
        try
        {
            net_client_->xio_send_read_request(ovs_aiocbp->aio_buf,
                                               ovs_aiocbp->aio_nbytes,
                                               ovs_aiocbp->aio_offset,
                                               reinterpret_cast<void*>(request));
        }
        catch (const volumedriverfs::XioClientQueueIsBusyException&)
        {
            errno = EBUSY;  r = -1;
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
    send_write_request(struct ovs_aiocb *ovs_aiocbp,
                       ovs_aio_request *request)
    {
        int r = 0;
        try
        {
            net_client_->xio_send_write_request(ovs_aiocbp->aio_buf,
                                                ovs_aiocbp->aio_nbytes,
                                                ovs_aiocbp->aio_offset,
                                                reinterpret_cast<void*>(request));
        }
        catch (const volumedriverfs::XioClientQueueIsBusyException&)
        {
            errno = EBUSY;  r = -1;
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
    send_flush_request(ovs_aio_request *request)
    {
        int r = 0;
        try
        {
            net_client_->xio_send_flush_request(reinterpret_cast<void*>(request));
        }
        catch (const volumedriverfs::XioClientQueueIsBusyException&)
        {
            errno = EBUSY;  r = -1;
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
    stat_volume(struct stat *st)
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
            volumedriverfs::NetworkXioClient::xio_stat_volume(uri_,
                                                              volname_,
                                                              static_cast<void*>(request.get()));
            errno = request->_errno;
            r = request->_errno ? -1 : 0;
            if (not request->_errno)
            {
                /* 512 for now */
                st->st_blksize = 512;
                st->st_size = request->_rv;
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
    allocate(size_t size)
    {
        ovs_buffer_t *buf;
        xio_reg_mem *mem = net_client_->allocate(size);
        if (mem)
        {
            buf = new ovs_buffer_t;
            buf->buf = mem->addr;
            buf->size = mem->length;
            buf->mem = mem;
            buf->from_mpool = true;
        }
        else
        {
            void *ptr;
            /* try to be on the safe side with 4k alignment */
            int ret = posix_memalign(&ptr, 4096, size);
            if (ret != 0)
            {
                errno = ret;
                buf = NULL;
            }
            else
            {
                buf = new ovs_buffer_t;
                buf->buf = ptr;
                buf->size = size;
                buf->from_mpool = false;
            }
        }
        return buf;
    }

    int
    deallocate(ovs_buffer_t *ptr)
    {
        if (ptr->from_mpool)
        {
            net_client_->deallocate(ptr->mem);
        }
        else
        {
            free (ptr->buf);
        }
        delete ptr;
        return 0;
    }
};

#endif //__NETWORK_XIO_CONTEXT_H
