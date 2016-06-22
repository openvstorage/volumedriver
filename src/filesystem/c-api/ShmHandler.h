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

#ifndef __SHM_HANDLER_H
#define __SHM_HANDLER_H

#include "VolumeCacheHandler.h"
#include "ShmControlChannelClient.h"
#include "ShmOrbClient.h"
#include "internal.h"

struct ovs_shm_context;

struct IOThread
{
    ovs_shm_context *shm_ctx_;
    std::thread iothread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped = false;
    bool stopping = false;

    void
    reset_iothread()
    {
        stopping = true;
        {
            std::unique_lock<std::mutex> lock_(mutex_);
            cv_.wait(lock_, [&]{ return stopped == true; });
        }
        iothread_.join();
    }

    void
    stop()
    {
        stopping = true;
    }
};

typedef std::unique_ptr<IOThread> IOThreadPtr;

static void _aio_readreply_handler(void*);
static void _aio_writereply_handler(void*);

struct ovs_shm_context
{
    ovs_shm_context(const ShmSegmentDetails& segment_details,
                    const std::string& volume_name,
                    int flag)
        : oflag(flag)
        , io_threads_pool_size_(youtils::System::get_env_with_default<int>("LIBOVSVOLUMEDRIVER_IO_THREADS_POOL_SIZE",
                                                                           1))
        , shm_client_(volumedriverfs::ShmOrbClient(segment_details).open(volume_name))
        , ctl_client_(std::make_shared<ShmControlChannelClient>(segment_details))
    {
        if (not ctl_client_->connect_and_register(volume_name,
                                                  shm_client_->get_key()))
        {
            throw fungi::IOException("cannot connect and register to server");
        }

        ovs_aio_init();

        try
        {
            cache_ = std::make_unique<VolumeCacheHandler>(*shm_client_,
                                                          ctl_client_);
        }
        catch (...)
        {
            ovs_aio_destroy();
            ctl_client_->deregister();
            throw;
        }
        int ret = cache_->preallocate();
        if (ret < 0)
        {
            cache_->drop_caches();
        }
    }

    ~ovs_shm_context()
    {
        ovs_aio_destroy();
        cache_.reset();
        shm_client_ = nullptr;
        ctl_client_->deregister();
    }

    void
    close_rr_iothreads()
    {
        for (auto& x: rr_iothreads)
        {
            x->reset_iothread();
        }
        rr_iothreads.clear();
    }
    void
    close_wr_iothreads()
    {
        for (auto& x: wr_iothreads)
        {
            x->reset_iothread();
        }
        wr_iothreads.clear();
    }

    void
    ovs_aio_init()
    {
        for (int i = 0; i < io_threads_pool_size_; i++)
        {
            IOThreadPtr iot;
            try
            {
                iot = std::make_unique<IOThread>();
                iot->shm_ctx_ = this;
                iot->iothread_ = std::thread(_aio_readreply_handler,
                                             (void*)iot.get());
                rr_iothreads.push_back(std::move(iot));
            }
            catch (...)
            {
                close_rr_iothreads();
                throw;
            }
        }

        for (int i = 0; i < io_threads_pool_size_; i++)
        {
            IOThreadPtr iot;
            try
            {
                iot = std::make_unique<IOThread>();
                iot->shm_ctx_ = this;
                iot->iothread_ = std::thread(_aio_writereply_handler,
                                             (void*)iot.get());
                wr_iothreads.push_back(std::move(iot));
            }
            catch (...)
            {
                close_wr_iothreads();
                close_rr_iothreads();
                throw;
            }
        }
    }

    void
    ovs_aio_destroy()
    {
        for (auto& iot: rr_iothreads)
        {
            iot->stop();
        }
        for (auto& iot: wr_iothreads)
        {
            iot->stop();
        }

        /* noexcept */
        if (ctl_client_->is_connected())
        {
            shm_client_->stop_reply_queues(io_threads_pool_size_);
        }
        close_wr_iothreads();
        close_rr_iothreads();
    }

    int oflag;
    int io_threads_pool_size_;
    std::unique_ptr<volumedriverfs::ShmClient> shm_client_;
    std::vector<IOThreadPtr> rr_iothreads;
    std::vector<IOThreadPtr> wr_iothreads;
    VolumeCacheHandlerPtr cache_;
    ShmControlChannelClientPtr ctl_client_;
};

static void
_aio_readreply_handler(void *arg)
{
    IOThread *iothread = (IOThread*) arg;
    ovs_shm_context *ctx = iothread->shm_ctx_;
    const struct timespec timeout = {2, 0};
    size_t size_in_bytes;
    bool failed;
    //cnanakos: stop thread by sending a stop request?
    while (not iothread->stopping)
    {
        ovs_aio_request *request;
        failed = ctx->shm_client_->timed_receive_read_reply(size_in_bytes,
                                                            reinterpret_cast<void**>(&request),
                                                            &timeout);
        if (request)
        {
            ovs_aio_request::handle_shm_request(request,
                                                size_in_bytes,
                                                failed);
        }
    }
    std::lock_guard<std::mutex> lock_(iothread->mutex_);
    iothread->stopped = true;
    iothread->cv_.notify_all();
}

static void
_aio_writereply_handler(void *arg)
{
    IOThread *iothread = (IOThread*) arg;
    ovs_shm_context *ctx = iothread->shm_ctx_;
    const struct timespec timeout = {2, 0};
    size_t size_in_bytes;
    bool failed;
    //cnanakos: stop thread by sending a stop request?
    while (not iothread->stopping)
    {
        ovs_aio_request *request;
        failed = ctx->shm_client_->timed_receive_write_reply(size_in_bytes,
                                                             reinterpret_cast<void**>(&request),
                                                             &timeout);
        if (request)
        {
            ovs_aio_request::handle_shm_request(request,
                                                size_in_bytes,
                                                failed);
        }
    }
    std::lock_guard<std::mutex> lock_(iothread->mutex_);
    iothread->stopped = true;
    iothread->cv_.notify_all();
}

#endif //SHM_HANDLER_H
