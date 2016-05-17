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

#ifndef VOLUME_THREAD_POOL_H
#define VOLUME_THREAD_POOL_H

#include "Types.h"
#include "VolumeDriverParameters.h"

#include <youtils/ThreadPool.h>
#include <youtils/WaitForIt.h>

namespace volumedriver
{
class VolumeInterface;
}

namespace youtils
{
template<>
struct ThreadPoolTraits<volumedriver::VolumeInterface*>
{
    // always insert failed tasks at the head of the queue
    static const bool
    requeue_before_first_barrier_on_error = false;

    // tasks before a barrier cannot be reordered
    static const bool
    may_reorder = false;

    static uint64_t
    sleep_microseconds_if_queue_is_inactive()
    {
        return 1000000;
    }

    typedef initialized_params::PARAMETER_TYPE(num_threads) number_of_threads_type;

    static const uint32_t max_number_of_threads = 256;

    static const char* component_name;

    // exponential backoff based on errorcount
    static uint64_t
    wait_microseconds_before_retry_after_error(uint32_t errorcount)
    {
        switch (errorcount)
        {
#define SECS * 1000000

        case 0: return 0 SECS;
        case 1: return 1 SECS;
        case 2: return 2 SECS;
        case 3: return 4 SECS;
        case 4: return 8 SECS;
        case 5: return 15 SECS;
        case 6: return 30 SECS;
        case 7: return 60 SECS;
        case 8 : return 120 SECS;
        case 9: return 240 SECS;
        default : return 300 SECS;

#undef SECS
        }
    }

    static volumedriver::VolumeInterface*
    default_producer_id()
    {
        return nullptr;
    }
};

}

namespace volumedriver
{

typedef youtils::ThreadPoolTraits<VolumeInterface*> VolumeThreadPoolTraits;

typedef youtils::ThreadPool<VolumeInterface*> VolPool;
typedef VolPool::Task VolPoolTask;

#define WAIT_FOR_THIS_VOLUME_WRITE(vol)                                 \
    ::youtils::WaitForIt<::volumedriver::VolPool>((vol), ::volumedriver::VolManager::get()->backend_thread_pool()).wait();

}


#endif // VOLUME_THREAD_POOL_H
