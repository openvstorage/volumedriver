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

#include "NetworkXioMempoolSlab.h"

#include <cmath>

namespace volumedriverfs
{

NetworkXioMempoolSlab::NetworkXioMempoolSlab(size_t block_size,
                                             size_t min_size,
                                             size_t max_size,
                                             size_t quantum_size,
                                             uint64_t index)
    : used_blocks(0)
    , total_blocks(0)
    , mb_size(block_size)
    , min(pow(2, ceil(log(min_size) / log(2))))
    , max(pow(2, ceil(log(max_size) / log(2))))
    , growing_quantum(pow(2, ceil(log(quantum_size) / log(2))))
    , slab_index(index)
    , region_index(0)
{
    min_slab_index = ++region_index;
    allocate_new_blocks(min_size);
}

NetworkXioMempoolSlab::~NetworkXioMempoolSlab()
{
    LOG_INFO("destroying mempool slab (mb size: " << mb_size
             << ", slab index: " << slab_index << ")");
    free_regions_locked();
}

void
NetworkXioMempoolSlab::free_regions_locked()
{
    boost::lock_guard<decltype(lock)> lock_(lock);
    assert(used_blocks == 0);
    for (const auto& b: blocks)
    {
        while (not b.second->empty())
        {
            auto *block = &b.second->front();
            b.second->pop_front();
            regions[block->region_index]->refcnt--;
            delete block;
            total_blocks--;
        }
    }

    assert(total_blocks == 0);
    for (const auto& r: regions) { assert(r.second->refcnt == 0); }
}

slab_mem_block*
NetworkXioMempoolSlab::try_alloc_block()
{
    for (const auto& b: blocks)
    {
        if (not b.second->empty())
        {
            slab_mem_block *block = &b.second->front();
            b.second->pop_front();
            regions[block->region_index]->refcnt++;
            used_blocks++;
            return block;
        }
    }
    return nullptr;
}

slab_mem_block*
NetworkXioMempoolSlab::alloc()
{
    boost::lock_guard<decltype(lock)> lock_(lock);
    auto block = try_alloc_block();
    if (not block)
    {
        try
        {
            allocate_new_blocks(growing_quantum);
        }
        catch (const std::bad_alloc&)
        {
            LOG_ERROR("failed to allocate new blocks, total blocks: " <<
                      total_blocks << ", used blocks: " << used_blocks <<
                      ", growing quantum: " << growing_quantum);
        }
    }
    else
    {
        return block;
    }
    return try_alloc_block();
}

void
NetworkXioMempoolSlab::free(slab_mem_block *mem_block)
{
    boost::lock_guard<decltype(lock)> lock_(lock);
    blocks[mem_block->region_index]->push_back(*mem_block);
    regions[mem_block->region_index]->refcnt--;
    used_blocks--;
}

void
NetworkXioMempoolSlab::allocate_new_blocks(size_t nr_blocks)
{
    if (total_blocks >= max)
    {
        LOG_ERROR("reached maximum number of total blocks, max blocks: "
                  << max);
        return;
    }
    auto region = std::make_unique<Region>(nr_blocks,
                                           mb_size,
                                           region_index);
    auto blocks_list = std::make_shared<BlocksList>();
    for (size_t i = 0; i < nr_blocks; i++)
    {
        try
        {
            slab_mem_block *block = new slab_mem_block(slab_index,
                                                       region_index);
            block->reg_mem.addr =
                static_cast<uint8_t*>(region->region_reg_mem.addr) + i*mb_size;
            block->reg_mem.length = mb_size;
            block->reg_mem.mr =  region->region_reg_mem.mr;
            blocks_list->push_back(*block);
            region->items++; total_blocks++;
        }
        catch (const std::bad_alloc&)
        {
            break;
        }
    }
    regions.emplace(std::make_pair(region_index, std::move(region)));
    blocks.emplace(std::make_pair(region_index, blocks_list));
    ++region_index;
}

void
NetworkXioMempoolSlab::clear_bucket(RegionBlocksBucketPtr bucket)
{
    while (not bucket->empty())
    {
        auto *block = &bucket->front();
        bucket->pop_front();
        delete block;
    }
}

void
NetworkXioMempoolSlab::try_free_unused_blocks()
{
    RegionBlocksBucketPtr bucket;
    {
        boost::lock_guard<decltype(lock)> lock_(lock);
        if (total_blocks > min)
        {
            for (const auto& r: regions)
            {
                if (r.second->refcnt == 0 &&
                    r.second->region_index != min_slab_index)
                {
                    LOG_DEBUG("removing "<< r.second->items << " blocks" <<
                              "(region: " << r.second->region_index <<
                              ", mb: " << mb_size << ")");
                    bucket = blocks[r.second->region_index];
                    total_blocks -= r.second->items;
                    regions.erase(r.second->region_index);
                    break;
                }
            }
        }
    }
    if (bucket)
    {
        clear_bucket(bucket);
    }
}

} //namespace
