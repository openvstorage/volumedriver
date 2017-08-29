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

#include "NetworkXioSlabConfig.h"

#include <youtils/StreamUtils.h>

namespace volumedriverfs
{

NetworkXioSlabConfig::NetworkXioSlabConfig(const size_t slab_block_sz,
                                           const size_t slab_init_blocks_nr,
                                           const size_t slab_grow_blocks_nr,
                                           const size_t slab_max_blocks_nr)
    : block_sz(slab_block_sz)
    , init_blocks_nr(slab_init_blocks_nr)
    , grow_blocks_nr(slab_grow_blocks_nr)
    , max_blocks_nr(slab_max_blocks_nr)
{}

bool
NetworkXioSlabConfig::operator==(const NetworkXioSlabConfig& other) const
{
    return block_sz == other.block_sz and
        init_blocks_nr == other.init_blocks_nr and
        grow_blocks_nr == other.grow_blocks_nr and
        max_blocks_nr == other.max_blocks_nr;
}

bool
NetworkXioSlabConfig::operator!=(const NetworkXioSlabConfig& other) const
{
    return not operator==(other);
}

std::ostream&
operator<<(std::ostream& os,
           const NetworkXioSlabConfig& cfg)
{
     return os <<
        "NetworkXioSlabConfig{block_sz=" << cfg.block_sz <<
        ",init_blocks_nr=" << cfg.init_blocks_nr <<
        ",grow_blocks_nr=" << cfg.grow_blocks_nr <<
        ",max_blocks_nr=" << cfg.max_blocks_nr <<
        "}";
}

std::ostream&
operator<<(std::ostream& os,
           const NetworkXioSlabConfigs& cfgs)
{
    return youtils::StreamUtils::stream_out_sequence(os, cfgs);
}

} //namespace volumedriverfs

namespace initialized_params
{

#define KEY(key, str)                           \
    const std::string                           \
    PropertyTreeVectorAccessor<volumedriverfs::NetworkXioSlabConfig>::key(str)

KEY(block_sz_key, "block_sz");
KEY(init_blocks_nr_key, "init_block_nr");
KEY(grow_blocks_nr_key, "grow_block_nr");
KEY(max_blocks_nr_key, "max_block_nr");

#undef KEY

} //namespace initialized_params
