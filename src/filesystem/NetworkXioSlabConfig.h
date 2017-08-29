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

#ifndef __NETWORK_XIO_SLAB_CONFIG_H_
#define __NETWORK_XIO_SLAB_CONFIG_H_

#include <youtils/InitializedParam.h>

namespace volumedriverfs
{

struct NetworkXioSlabConfig
{
    NetworkXioSlabConfig(const size_t slab_block_sz,
                         const size_t slab_init_blocks_nr,
                         const size_t slab_grow_blocks_nr,
                         const size_t slab_max_blocks_nr);

    ~NetworkXioSlabConfig() = default;

    NetworkXioSlabConfig(const NetworkXioSlabConfig&) = default;

    NetworkXioSlabConfig&
    operator=(const NetworkXioSlabConfig&) = default;

    bool
    operator==(const NetworkXioSlabConfig&) const;

    bool
    operator!=(const NetworkXioSlabConfig&) const;

    size_t block_sz;
    size_t init_blocks_nr;
    size_t grow_blocks_nr;
    size_t max_blocks_nr;

    static std::vector<NetworkXioSlabConfig>
    DefaultSlabsConfig()
    {
        std::vector<NetworkXioSlabConfig> cfg;
        cfg.push_back(NetworkXioSlabConfig(4096, 2048, 8192, 131072));
        cfg.push_back(NetworkXioSlabConfig(8192, 2048, 4096, 131072));
        cfg.push_back(NetworkXioSlabConfig(16384, 2048, 4096, 131072));
        cfg.push_back(NetworkXioSlabConfig(32768, 2048, 2048, 131072));
        cfg.push_back(NetworkXioSlabConfig(65536, 0, 2048, 65536));
        cfg.push_back(NetworkXioSlabConfig(131072, 0, 512, 16384));

        assert(cfg.size() == 6);
        return cfg;
    }
};

using NetworkXioSlabConfigs = std::vector<NetworkXioSlabConfig>;

std::ostream&
operator<<(std::ostream& os,
           const NetworkXioSlabConfig& cfg);

std::ostream&
operator<<(std::ostream& os,
           const NetworkXioSlabConfigs& cfgs);

} //namespace volumedriverfs

namespace initialized_params
{

template<>
struct PropertyTreeVectorAccessor<volumedriverfs::NetworkXioSlabConfig>
{
    using Type = volumedriverfs::NetworkXioSlabConfig;

    static const std::string block_sz_key;
    static const std::string init_blocks_nr_key;
    static const std::string grow_blocks_nr_key;
    static const std::string max_blocks_nr_key;

    static Type
    get(const boost::property_tree::ptree& pt)
    {
        return Type(pt.get<size_t>(block_sz_key),
                    pt.get<size_t>(init_blocks_nr_key),
                    pt.get<size_t>(grow_blocks_nr_key),
                    pt.get<size_t>(max_blocks_nr_key));
    }

    static void
    put(boost::property_tree::ptree& pt,
        const Type& cfg)
    {
        pt.put(block_sz_key,
               cfg.block_sz);
        pt.put(init_blocks_nr_key,
               cfg.init_blocks_nr);
        pt.put(grow_blocks_nr_key,
               cfg.grow_blocks_nr);
        pt.put(max_blocks_nr_key,
               cfg.max_blocks_nr);
    }
};

} //namespace initialized_params

#endif // __NETWORK_XIO_SLAB_CONFIG_H_
