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

#include "NetworkXioMempool.h"

#include <youtils/System.h>

namespace volumedriverfs
{

namespace yt = youtils;

NetworkXioMempool::NetworkXioMempool()
    : slab_index(0)
    , stopping(false)
{
    try
    {
        slabs_manager_thread = std::thread(&NetworkXioMempool::slabs_manager,
                                           this);
    }
    catch (const std::system_error& e)
    {
        throw FailedCreateMempoolThread("failed to create manager thread");
    }
}

NetworkXioMempool::~NetworkXioMempool()
{
    {
        std::lock_guard<std::mutex> lock_(mutex_);
        stopping = true;
        cv_.notify_one();
    }
    slabs_manager_thread.join();
}

void
NetworkXioMempool::add_slab(size_t block_size,
                            size_t min,
                            size_t max,
                            size_t quantum)
{
    for (const auto& s: slabs)
    {
        if (s.second->get_mb_size() == block_size)
        {
            LOG_ERROR("failed to add slab with block size: " << block_size);
            throw FailedCreateMempoolSlab("failed to add slab");
        }
    }
    auto slab = std::make_unique<NetworkXioMempoolSlab>(block_size,
                                                        min,
                                                        max,
                                                        quantum,
                                                        ++slab_index);
    slabs.emplace(std::make_pair(slab->get_index(), std::move(slab)));
}

slab_mem_block*
NetworkXioMempool::alloc(size_t length)
{
    if (not slabs.empty())
    {
        for (const auto& s: slabs)
        {
            if (length <= s.second->get_mb_size())
            {
                return s.second->alloc();
            }
        }
    }
    LOG_ERROR("failed to find available slabs");
    return nullptr;
}

void
NetworkXioMempool::free(slab_mem_block *mem_block)
{
    if (mem_block)
    {
        slabs[mem_block->slab_index]->free(mem_block);
    }
}

void
NetworkXioMempool::slabs_manager()
{
    using System = yt::System;
    using Clock = std::chrono::steady_clock;

    std::string c_intvl_env("NETWORK_XIO_SLAB_MANAGER_CHECK_INTERVAL_MINS");

    auto sleep_time = std::chrono::milliseconds(500UL);
    auto time_point_interval =
        std::chrono::minutes(System::get_env_with_default(c_intvl_env, 15UL));
    auto time_point = Clock::time_point::min();
    pthread_setname_np(pthread_self(), "slab_manager");
    while (true)
    {
        std::unique_lock<std::mutex> lock_(mutex_);
        const bool ret = cv_.wait_for(lock_,
                                      sleep_time,
                                      [&]{return stopping;});
        if (ret)
        {
            break;
        }
        else
        {
            if (time_point + time_point_interval <= Clock::now())
            {
                for (const auto& s: slabs)
                {
                    s.second->try_free_unused_blocks();
                }
                time_point = Clock::now();
            }
        }
    }
}

} //namespace
