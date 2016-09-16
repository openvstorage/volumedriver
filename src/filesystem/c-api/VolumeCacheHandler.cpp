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

#include "VolumeCacheHandler.h"

#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/thread.hpp>

#include <youtils/SpinLock.h>
#include <queue>

namespace vfs = volumedriverfs;

VolumeCacheHandler::VolumeCacheHandler(vfs::ShmClientPtr shm_client,
                                       ShmControlChannelClientPtr ctl_client)
    : shm_client_(shm_client)
    , ctl_client_(ctl_client)
{
}

VolumeCacheHandler::~VolumeCacheHandler()
{
    drop_caches();
}

ovs_buffer_t*
VolumeCacheHandler::allocate(size_t size)
{
    int retries = 15;
    ovs_buffer_t *buffer;
    boost::posix_time::time_duration delay =
        boost::posix_time::nanoseconds(10);
    while ((buffer = _try_allocate(size)) == NULL)
    {
        boost::this_thread::sleep(delay);
        retries--;
        delay *= 2;
        if (retries == 0)
        {
            break;
        }
    }
    return buffer;
}

int
VolumeCacheHandler::deallocate(ovs_buffer_t *shptr)
{
    if (shptr)
    {
        switch (shptr->size)
        {
        case BufferSize::s_4k:
            return _maybe_deallocate_to_queue(shptr,
                                              chunks_4k,
                                              lock_4k,
                                              QueueSize::qs_4k);
        case BufferSize::s_32k:
            return _maybe_deallocate_to_queue(shptr,
                                              chunks_32k,
                                              lock_32k,
                                              QueueSize::qs_32k);
        case BufferSize::s_128k:
            return _maybe_deallocate_to_queue(shptr,
                                              chunks_128k,
                                              lock_128k,
                                              QueueSize::qs_128k);
        default:
            void *ptr = shptr->buf;
            delete shptr;
            return ctl_channel_deallocate(ptr);
        }
    }
    else
    {
        return -1;
    }
}

int
VolumeCacheHandler::preallocate()
{
    for (int i = 0; i < QueueSize::qs_4k; i++)
    {
        void *ptr = ctl_channel_allocate(BufferSize::s_4k);
        if (ptr)
        {
            chunks_4k.push(ptr);
        }
        else
        {
            return -1;
        }
    }
    for (int i = 0; i < QueueSize::qs_32k; i++)
    {
        void *ptr = ctl_channel_allocate(BufferSize::s_32k);
        if (ptr)
        {
            chunks_32k.push(ptr);
        }
        else
        {
            return -1;
        }
    }
    for (int i = 0; i < QueueSize::qs_128k; i++)
    {
        void *ptr = ctl_channel_allocate(BufferSize::s_128k);
        if (ptr)
        {
            chunks_128k.push(ptr);
        }
        else
        {
            return -1;
        }
    }
    return 0;
}

void
VolumeCacheHandler::drop_caches()
{
    {
        fungi::ScopedSpinLock lock_(lock_4k);
        while (not chunks_4k.empty())
        {
            void *buf = chunks_4k.front();
            chunks_4k.pop();
            ctl_channel_deallocate(buf);
        }
    }
    {
        fungi::ScopedSpinLock lock_(lock_32k);
        while (not chunks_32k.empty())
        {
            void *buf = chunks_32k.front();
            chunks_32k.pop();
            ctl_channel_deallocate(buf);
        }
    }
    {
        fungi::ScopedSpinLock lock_(lock_128k);
        while (not chunks_128k.empty())
        {
            void *buf = chunks_128k.front();
            chunks_128k.pop();
            ctl_channel_deallocate(buf);
        }
    }
}

ovs_buffer_t*
VolumeCacheHandler::_try_allocate(size_t size)
{
    void *buf = NULL;
    size_t max = BufferSize::s_4k;
    buf = _maybe_allocate_from_queue(size,
                                     chunks_4k,
                                     lock_4k,
                                     max);
    if (not buf)
    {
        max = BufferSize::s_32k;
        buf = _maybe_allocate_from_queue(size,
                                         chunks_32k,
                                         lock_32k,
                                         max);
    }
    if (not buf)
    {
        max = BufferSize::s_128k;
        buf = _maybe_allocate_from_queue(size,
                                         chunks_128k,
                                         lock_128k,
                                         max);
    }
    if (not buf)
    {
        max = size;
        buf = ctl_channel_allocate(size);
    }
    if (buf)
    {
        ovs_buffer_t *ovs_buff = new ovs_buffer_t;
        ovs_buff->buf = buf;
        ovs_buff->size = max;
        return ovs_buff;
    }
    else
    {
        return NULL;
    }
}

void*
VolumeCacheHandler::ctl_channel_allocate(size_t size)
{
    boost::interprocess::managed_shared_memory::handle_t handle;
    bool ret = ctl_client_->allocate(handle, size);
    if (ret)
    {
        void *ptr = shm_client_->get_address_from_handle(handle);
        return ptr;
    }
    return NULL;
}

int
VolumeCacheHandler::ctl_channel_deallocate(void *buf)
{
    boost::interprocess::managed_shared_memory::handle_t handle =
        shm_client_->get_handle_from_address(buf);
    return (ctl_client_->deallocate(handle) ? 0 : -1);
}

void*
VolumeCacheHandler::_maybe_allocate_from_queue(size_t size,
                                               BufferQueue& queue,
                                               fungi::SpinLock& lock_,
                                               size_t max)
{
    if (size <= max)
    {
        {
            fungi::ScopedSpinLock queue_lock(lock_);
            if (not queue.empty())
            {
                void *buf = queue.front();
                queue.pop();
                return buf;
            }
        }
        return ctl_channel_allocate(max);
    }
    else
    {
        return NULL;
    }
}

int
VolumeCacheHandler::_maybe_deallocate_to_queue(ovs_buffer_t *shptr,
                                               BufferQueue& queue,
                                               fungi::SpinLock& lock_,
                                               size_t queue_size)
{
    void *ptr = shptr->buf;
    delete shptr;

    lock_.lock();
    if (queue.size() < queue_size + 1)
    {
        queue.push(ptr);
        lock_.unlock();
    }
    else
    {
        lock_.unlock();
        return ctl_channel_deallocate(ptr);
    }
    return 0;
}
