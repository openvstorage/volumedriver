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

#ifndef _VOLUME_CACHE_HANDLER_H
#define _VOLUME_CACHE_HANDLER_H

#include "volumedriver.h"
#include "common.h"
#include "ShmClient.h"
#include "ShmControlChannelClient.h"

#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/thread.hpp>

#include <youtils/SpinLock.h>
#include <queue>

class VolumeCacheHandler
{
public:
    VolumeCacheHandler(libovsvolumedriver::ShmClientPtr shm_client,
                       ShmControlChannelClientPtr ctl_client);

    ~VolumeCacheHandler();

    VolumeCacheHandler(const VolumeCacheHandler&) = delete;
    VolumeCacheHandler& operator=(const VolumeCacheHandler&) = delete;

    ovs_buffer_t*
    allocate(size_t size);

    int
    deallocate(ovs_buffer_t *shptr);

    int
    preallocate();

    void
    drop_caches();

private:
    typedef std::queue<void*> BufferQueue;
    libovsvolumedriver::ShmClientPtr shm_client_;
    ShmControlChannelClientPtr ctl_client_;

    enum BufferSize
    {
        s_4k = 4096,
        s_32k = 32768,
        s_128k = 131072,
    };

    enum QueueSize
    {
        qs_4k = 256,
        qs_32k = 32,
        qs_128k = 4,
    };

    BufferQueue chunks_4k;
    BufferQueue chunks_32k;
    BufferQueue chunks_128k;
    fungi::SpinLock lock_4k;
    fungi::SpinLock lock_32k;
    fungi::SpinLock lock_128k;

    ovs_buffer_t*
    _try_allocate(size_t size);

    void*
    ctl_channel_allocate(size_t size);

    int
    ctl_channel_deallocate(void *buf);

    void*
    _maybe_allocate_from_queue(size_t size,
                               BufferQueue& queue,
                               fungi::SpinLock& lock_,
                               size_t max);

    int
    _maybe_deallocate_to_queue(ovs_buffer_t *shptr,
                               BufferQueue& queue,
                               fungi::SpinLock& lock_,
                               size_t queue_size);
};

typedef std::unique_ptr<VolumeCacheHandler> VolumeCacheHandlerPtr;

#endif //_VOLUME_CACHE_HANDLER_H
