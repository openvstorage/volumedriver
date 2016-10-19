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
#include "ShmHandler.h"

#include <sstream>
#include <boost/type_index.hpp>

namespace bti =  boost::typeindex;

ovs_shm_context::ovs_shm_context(const std::string& volume_name,
                                 int flag)
    : oflag(flag)
{
    const std::string io_env_var("LIBOVSVOLUMEDRIVER_IO_THREADS_POOL_SIZE");
    io_threads_pool_size_ =
                youtils::System::get_env_with_default<int>(io_env_var, 1);
    shm_client_ = std::make_shared<volumedriverfs::ShmClient>(volume_name);
    try
    {
        ctl_client_ = std::make_shared<ShmControlChannelClient>();
    }
    catch (...)
    {
        shm_client_.reset();
        throw;
    }

    if (not ctl_client_->connect_and_register(volume_name,
                                              shm_client_->get_key()))
    {
        shm_client_.reset();
        ctl_client_.reset();
        LIBLOGID_ERROR("cannot connect ang register to server");
        throw fungi::IOException("cannot connect and register to server");
    }

    try
    {
        ovs_aio_init();
    }
    catch (...)
    {
        LIBLOGID_ERROR("failed to init aio");
        shm_client_.reset();
        ctl_client_.reset();
        throw;
    }

    try
    {
        cache_ = std::make_unique<VolumeCacheHandler>(shm_client_,
                                                      ctl_client_);
    }
    catch (...)
    {
        LIBLOGID_ERROR("failed to create volume cache handler");
        ovs_aio_destroy();
        shm_client_.reset();
        ctl_client_->deregister();
        ctl_client_.reset();
        throw;
    }
    int ret = cache_->preallocate();
    if (ret < 0)
    {
        LIBLOGID_INFO("dropped caches");
        cache_->drop_caches();
    }
}

ovs_shm_context::~ovs_shm_context()
{
    ovs_aio_destroy();
    cache_.reset();
    shm_client_.reset();
    ctl_client_->deregister();
    ctl_client_.reset();
}

const std::string
ovs_shm_context::get_log_identifier()
{
    std::ostringstream os;
    os << bti::type_id_runtime(*this).pretty_name() << "(" << this << ")";
    return os.str();
}

void
ovs_shm_context::close_rr_iothreads()
{
    for (auto& x: rr_iothreads)
    {
        x->reset_iothread();
    }
    rr_iothreads.clear();
}

void
ovs_shm_context::close_wr_iothreads()
{
    for (auto& x: wr_iothreads)
    {
        x->reset_iothread();
    }
    wr_iothreads.clear();
}

void
ovs_shm_context::ovs_aio_init()
{
    for (int i = 0; i < io_threads_pool_size_; i++)
    {
        IOThreadPtr iot;
        try
        {
            iot = std::make_unique<IOThread>();
            iot->iothread_ = std::thread(&ovs_shm_context::rr_handler,
                                         this,
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
            iot->iothread_ = std::thread(&ovs_shm_context::wr_handler,
                                         this,
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
ovs_shm_context::ovs_aio_destroy()
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

void
ovs_shm_context::rr_handler(void *arg)
{
    IOThread *iothread = (IOThread*) arg;
    const struct timespec timeout = {2, 0};
    size_t size_in_bytes;
    bool failed;
    //cnanakos: stop thread by sending a stop request?
    while (not iothread->stopping)
    {
        ovs_aio_request *request;
        failed = shm_client_->timed_receive_read_reply(size_in_bytes,
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

void
ovs_shm_context::wr_handler(void *arg)
{
    IOThread *iothread = (IOThread*) arg;
    const struct timespec timeout = {2, 0};
    size_t size_in_bytes;
    bool failed;
    //cnanakos: stop thread by sending a stop request?
    while (not iothread->stopping)
    {
        ovs_aio_request *request;
        failed = shm_client_->timed_receive_write_reply(size_in_bytes,
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
