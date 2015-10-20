// Copyright 2015 Open vStorage NV
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

#include "RocksConfig.h"

#include <boost/optional/optional_io.hpp>

#include <youtils/RocksLogger.h>

namespace metadata_server
{

namespace rdb = rocksdb;
namespace yt = youtils;

bool
RocksConfig::operator==(const RocksConfig& other) const
{
    return db_threads == other.db_threads and
        write_cache_size == other.write_cache_size and
        read_cache_size == other.read_cache_size and
        enable_wal == other.enable_wal and
        data_sync == other.data_sync;
}

// pretty much arbitrarily chosen values for now (also applies to
// column_family_options() below) - revisit to see what we want to make
// configurable from the outside
rocksdb::DBOptions
RocksConfig::db_options(const std::string& id) const
{
    rdb::DBOptions opts;

    opts.IncreaseParallelism(db_threads ?
                             db_threads->t :
                             ::sysconf(_SC_NPROCESSORS_ONLN));
    opts.disableDataSync = data_sync ? *data_sync == DataSync::F : true;
    opts.paranoid_checks = false;
    opts.create_if_missing = true;
    opts.info_log = std::make_shared<yt::RocksLogger>(id);
    return opts;

}

rdb::ColumnFamilyOptions
RocksConfig::column_family_options() const
{
    rdb::ColumnFamilyOptions opts;

    opts.OptimizeLevelStyleCompaction();

    // we don't need no iterators, no!
    opts.OptimizeForPointLookup(read_cache_size ?
                                read_cache_size->t >> 20 :
                                4);

    if (write_cache_size)
    {
        opts.write_buffer_size = write_cache_size->t >> 20; // default: 4 MiB
    }

    // moved to BlockBasedTableOptions
    // opts.block_size = vd::CachePage::size();
    // default: kSnappyCompression, which is claimed to be fast enough
    opts.compression = rdb::CompressionType::kNoCompression;
    opts.verify_checksums_in_compaction = false;

    return opts;
}

rdb::ReadOptions
RocksConfig::read_options() const
{
    rdb::ReadOptions opts;
    opts.verify_checksums = false;
    return opts;
}

rdb::WriteOptions
RocksConfig::write_options() const
{
    rdb::WriteOptions opts;
    opts.disableWAL = enable_wal ? *enable_wal == EnableWal::F : true;
    opts.sync = data_sync ? *data_sync == DataSync::T : false;
    return opts;
}

std::ostream&
operator<<(std::ostream& os,
           const RocksConfig& cfg)
{
    return os <<
        "RocksConfig{db_threads=" << cfg.db_threads <<
        ",write_cache_size=" << cfg.write_cache_size <<
        ",read_cache_size=" << cfg.read_cache_size <<
        ",enable_wal=" << cfg.enable_wal <<
        ",data_sync=" << cfg.data_sync <<
        "}";
}

}
