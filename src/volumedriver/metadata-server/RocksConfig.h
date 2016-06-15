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

#ifndef MDS_ROCKS_CONFIG_H_
#define MDS_ROCKS_CONFIG_H_

#include <boost/optional.hpp>

#include <rocksdb/options.h>

#include <youtils/BooleanEnum.h>
#include <youtils/OurStrongTypedef.h>

OUR_STRONG_ARITHMETIC_TYPEDEF(size_t,
                              WriteCacheSize,
                              metadata_server);

OUR_STRONG_ARITHMETIC_TYPEDEF(size_t,
                              ReadCacheSize,
                              metadata_server);

OUR_STRONG_ARITHMETIC_TYPEDEF(size_t,
                              DbThreads,
                              metadata_server);

namespace metadata_server
{

BOOLEAN_ENUM(EnableWal);
BOOLEAN_ENUM(DataSync);

struct RocksConfig
{
    enum class CompactionStyle
    {
        Level,
        Universal,
        Fifo
            // None is not exposed for now while we don't invoke compaction ourselves
    };

    boost::optional<DbThreads> db_threads;
    boost::optional<WriteCacheSize> write_cache_size;
    boost::optional<ReadCacheSize> read_cache_size;
    boost::optional<EnableWal> enable_wal;
    boost::optional<DataSync> data_sync;
    boost::optional<int> max_write_buffer_number;
    boost::optional<int> min_write_buffer_number_to_merge;
    boost::optional<int> num_levels;
    boost::optional<int> level0_file_num_compaction_trigger;
    boost::optional<int> level0_slowdown_writes_trigger;
    boost::optional<int> level0_stop_writes_trigger;
    // max_mem_compaction_level?
    boost::optional<uint64_t> target_file_size_base;
    // target_file_size_multiplier?
    boost::optional<uint64_t> max_bytes_for_level_base;
    // level_compaction_dynamic_level_bytes:
    // not to be reconfigured for existing DBs so we leave it alone (false)
    // max_bytes_for_level_multiplier?
    // max_bytes_for_level_multiplier_additional?
    // expanded_compaction_factor?
    // source_compaction_factor?
    // max_grandparent_overlap_factor?
    // soft_rate_limit?
    // hard_rate_limit?
    // rate_limit_delay_max_milliseconds?
    // arena_block_size?
    // disable_auto_compactions? not while we don't trigger them manually ...
    // purge_redundant_kvs_while_flush?
    boost::optional<CompactionStyle> compaction_style;
    // verify_checksums_in_compaction?
    // filter_deletes? we don't do these without TRIM support

    RocksConfig() = default;

    ~RocksConfig() = default;

    RocksConfig(const RocksConfig&) = default;

    RocksConfig&
    operator=(const RocksConfig&) = default;

    RocksConfig(RocksConfig&&) = default;

    RocksConfig&
    operator=(RocksConfig&&) = default;

    bool
    operator==(const RocksConfig&) const;

    bool
    operator!=(const RocksConfig& other) const
    {
        return not operator==(other);
    }

    rocksdb::DBOptions
    db_options(const std::string& id) const;

    rocksdb::ColumnFamilyOptions
    column_family_options() const;

    rocksdb::ReadOptions
    read_options() const;

    rocksdb::WriteOptions
    write_options() const;
};

std::ostream&
operator<<(std::ostream&,
           const RocksConfig::CompactionStyle);

std::istream&
operator>>(std::istream&,
           RocksConfig::CompactionStyle&);

std::ostream&
operator<<(std::ostream&,
           const RocksConfig&);
}

#endif // !MDS_ROCKSCONFIG_H_
