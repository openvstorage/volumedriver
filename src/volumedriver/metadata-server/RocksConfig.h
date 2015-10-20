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
    boost::optional<DbThreads> db_threads;
    boost::optional<WriteCacheSize> write_cache_size;
    boost::optional<ReadCacheSize> read_cache_size;
    boost::optional<EnableWal> enable_wal;
    boost::optional<DataSync> data_sync;

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
           const RocksConfig&);
}

#endif // !MDS_ROCKSCONFIG_H_
