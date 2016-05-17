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

#ifndef MDS_SERVER_CONFIG_H_
#define MDS_SERVER_CONFIG_H_

#include "RocksConfig.h"

#include "../MDSNodeConfig.h"

#include <youtils/InitializedParam.h>

namespace metadata_server
{

struct ServerConfig
{
    ServerConfig(const volumedriver::MDSNodeConfig& ncfg,
                 const boost::filesystem::path& db,
                 const boost::filesystem::path& scratch,
                 const RocksConfig& = RocksConfig());

    ~ServerConfig() = default;

    ServerConfig(const ServerConfig&) = default;

    ServerConfig&
    operator=(const ServerConfig&) = default;

    bool
    operator==(const ServerConfig&) const;

    bool
    operator!=(const ServerConfig&) const;

    bool
    conflicts(const ServerConfig&) const;

    volumedriver::MDSNodeConfig node_config;
    boost::filesystem::path db_path;
    boost::filesystem::path scratch_path;
    RocksConfig rocks_config;
};

using ServerConfigs = std::vector<ServerConfig>;

std::ostream&
operator<<(std::ostream& os,
           const ServerConfig& cfg);

std::ostream&
operator<<(std::ostream& os,
           const ServerConfigs& cfgs);

}

namespace initialized_params
{

template<>
struct PropertyTreeVectorAccessor<metadata_server::ServerConfig>
{
    using Type = metadata_server::ServerConfig;

    static const std::string db_path_key;
    static const std::string scratch_path_key;
    static const std::string db_threads_key;
    static const std::string write_cache_size_key;
    static const std::string read_cache_size_key;
    static const std::string enable_wal_key;
    static const std::string data_sync_key;
    static const std::string max_write_buffer_number_key;
    static const std::string min_write_buffer_number_to_merge_key;
    static const std::string num_levels_key;
    static const std::string level0_file_num_compaction_trigger_key;
    static const std::string level0_slowdown_writes_trigger_key;
    static const std::string level0_stop_writes_trigger_key;
    static const std::string target_file_size_base_key;
    static const std::string max_bytes_for_level_base_key;
    static const std::string compaction_style_key;

    static Type
    get(const boost::property_tree::ptree& pt)
    {
        using namespace metadata_server;

        const auto
            ncfg(PropertyTreeVectorAccessor<volumedriver::MDSNodeConfig>::get(pt));

        RocksConfig rcfg;

        // Obvious candidate for a preprocessor macro, but how can one get at the
        // boost::optional<T>'s T ?
        rcfg.db_threads = pt.get_optional<DbThreads>(db_threads_key);
        rcfg.write_cache_size = pt.get_optional<WriteCacheSize>(write_cache_size_key);
        rcfg.read_cache_size = pt.get_optional<ReadCacheSize>(read_cache_size_key);
        rcfg.enable_wal = pt.get_optional<EnableWal>(enable_wal_key);
        rcfg.data_sync = pt.get_optional<DataSync>(data_sync_key);
        rcfg.max_write_buffer_number =
            pt.get_optional<int>(max_write_buffer_number_key);
        rcfg.min_write_buffer_number_to_merge =
            pt.get_optional<int>(min_write_buffer_number_to_merge_key);
        rcfg.num_levels = pt.get_optional<int>(num_levels_key);
        rcfg.level0_file_num_compaction_trigger =
            pt.get_optional<int>(level0_file_num_compaction_trigger_key);
        rcfg.level0_slowdown_writes_trigger =
            pt.get_optional<int>(level0_slowdown_writes_trigger_key);
        rcfg.level0_stop_writes_trigger =
            pt.get_optional<int>(level0_stop_writes_trigger_key);
        rcfg.target_file_size_base =
            pt.get_optional<uint64_t>(target_file_size_base_key);
        rcfg.max_bytes_for_level_base =
            pt.get_optional<uint64_t>(max_bytes_for_level_base_key);
        rcfg.compaction_style =
            pt.get_optional<RocksConfig::CompactionStyle>(compaction_style_key);

        return Type(ncfg,
                    pt.get<boost::filesystem::path>(db_path_key),
                    pt.get<boost::filesystem::path>(scratch_path_key),
                    rcfg);
    }

    static void
    put(boost::property_tree::ptree& pt,
        const Type& cfg)
    {
        using namespace metadata_server;

        PropertyTreeVectorAccessor<volumedriver::MDSNodeConfig>::put(pt,
                                                                     cfg.node_config);
        pt.put(db_path_key,
               cfg.db_path);
        pt.put(scratch_path_key,
               cfg.scratch_path);

#define P(OPT)                                               \
        if (cfg.rocks_config.OPT)                            \
        {                                                    \
            pt.put(OPT ## _key,                              \
                   *cfg.rocks_config.OPT);                   \
        }

        P(db_threads);
        P(write_cache_size);
        P(read_cache_size);
        P(enable_wal);
        P(data_sync);
        P(max_write_buffer_number);
        P(min_write_buffer_number_to_merge);
        P(num_levels);
        P(level0_file_num_compaction_trigger);
        P(level0_slowdown_writes_trigger);
        P(level0_stop_writes_trigger);
        P(target_file_size_base);
        P(max_bytes_for_level_base);
        P(compaction_style);

#undef P
    }
};

}

#endif //!MDS_SERVER_CONFIG_H_
