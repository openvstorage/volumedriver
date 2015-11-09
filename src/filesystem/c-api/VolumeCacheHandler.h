// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _VOLUME_CACHE_H
#define _VOLUME_CACHE_H

#include "../ShmClient.h"

#include <youtils/SpinLock.h>
#include <queue>

class VolumeCacheHandler
{
public:
    VolumeCacheHandler(void *shm_handle)
    : shm_handle_(shm_handle)
    {}

    ovs_buffer_t*
    allocate(size_t size)
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
            buf = shm_allocate(static_cast<ShmClientHandle>(shm_handle_),
                               size);
        }
        if (buf)
        {
            ovs_buffer_t *ovs_buff = new ovs_buffer_t();
            ovs_buff->buf = buf;
            ovs_buff->size = max;
            return ovs_buff;
        }
        else
        {
            errno = ENOMEM;
            return NULL;
        }
    }

    int
    deallocate(ovs_buffer_t *shptr)
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
                shm_deallocate(static_cast<ShmClientHandle>(shm_handle_),
                               shptr->buf);
                delete shptr;
                return 0;
            }
        }
        else
        {
            errno = EFAULT;
            return -1;
        }
    }

    int
    preallocate()
    {
        for (int i = 0; i < QueueSize::qs_4k; i++)
        {
            void *ptr = shm_allocate(static_cast<ShmClientHandle>(shm_handle_),
                                     BufferSize::s_4k);
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
            void *ptr = shm_allocate(static_cast<ShmClientHandle>(shm_handle_),
                                     BufferSize::s_32k);
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
            void *ptr = shm_allocate(static_cast<ShmClientHandle>(shm_handle_),
                                     BufferSize::s_128k);
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
    drop_caches()
    {
        {
            fungi::ScopedSpinLock lock_(lock_4k);
            while (not chunks_4k.empty())
            {
                void *buf = chunks_4k.front();
                chunks_4k.pop();
                shm_deallocate(static_cast<ShmClientHandle>(shm_handle_),
                               buf);
            }
        }
        {
            fungi::ScopedSpinLock lock_(lock_32k);
            while (not chunks_32k.empty())
            {
                void *buf = chunks_32k.front();
                chunks_32k.pop();
                shm_deallocate(static_cast<ShmClientHandle>(shm_handle_),
                               buf);
            }
        }
        {
            fungi::ScopedSpinLock lock_(lock_128k);
            while (not chunks_128k.empty())
            {
                void *buf = chunks_128k.front();
                chunks_128k.pop();
                shm_deallocate(static_cast<ShmClientHandle>(shm_handle_),
                               buf);
            }
        }
    }

private:
    typedef std::queue<void*> BufferQueue;
    void *shm_handle_;

    enum BufferSize
    {
        s_4k = 4096,
        s_32k = 32768,
        s_128k = 131072,
    };

    enum QueueSize
    {
        qs_4k = 512,
        qs_32k = 48,
        qs_128k = 8,
    };

    BufferQueue chunks_4k;
    BufferQueue chunks_32k;
    BufferQueue chunks_128k;
    fungi::SpinLock lock_4k;
    fungi::SpinLock lock_32k;
    fungi::SpinLock lock_128k;

    void*
    _maybe_allocate_from_queue(size_t size,
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
            return shm_allocate(static_cast<ShmClientHandle>(shm_handle_),
                                max);
        }
        else
        {
            return NULL;
        }
    }

    int
    _maybe_deallocate_to_queue(ovs_buffer_t *shptr,
                               BufferQueue& queue,
                               fungi::SpinLock& lock_,
                               size_t queue_size)
    {
        lock_.lock();
        if (queue.size() < queue_size + 1)
        {
            queue.push(shptr->buf);
            lock_.unlock();
        }
        else
        {
            lock_.unlock();
            shm_deallocate(static_cast<ShmClientHandle>(shm_handle_),
                           shptr->buf);
        }
        delete shptr;
        return 0;
    }
};

typedef std::unique_ptr<VolumeCacheHandler> VolumeCacheHandlerPtr;

#endif //_VOLUME_CACHE_H
