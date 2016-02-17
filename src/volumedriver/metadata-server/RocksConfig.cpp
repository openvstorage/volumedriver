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

#include "RocksConfig.h"

#include <boost/bimap.hpp>
#include <boost/optional/optional_io.hpp>

#include <youtils/RocksLogger.h>
#include <youtils/StreamUtils.h>

namespace metadata_server
{

namespace rdb = rocksdb;
namespace yt = youtils;


bool
RocksConfig::operator==(const RocksConfig& other) const
{
#define C(x)                                    \
    x == other.x

    return
        C(db_threads) and
        C(write_cache_size) and
        C(read_cache_size) and
        C(enable_wal) and
        C(data_sync) and
        C(max_write_buffer_number) and
        C(min_write_buffer_number_to_merge) and
        C(num_levels) and
        C(level0_file_num_compaction_trigger) and
        C(level0_slowdown_writes_trigger) and
        C(level0_stop_writes_trigger) and
        C(target_file_size_base) and
        C(max_bytes_for_level_base) and
        C(compaction_style);

#undef C
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

#define S(OPT)                                  \
    if (OPT)                                    \
    {                                           \
        opts.OPT = *OPT;                        \
    }

    S(max_write_buffer_number);
    S(min_write_buffer_number_to_merge);
    S(num_levels);
    S(level0_file_num_compaction_trigger);
    S(level0_slowdown_writes_trigger);
    S(level0_stop_writes_trigger);
    S(target_file_size_base);
    S(max_bytes_for_level_base);

#undef S

    if (compaction_style)
    {
        switch (*compaction_style)
        {
        case RocksConfig::CompactionStyle::Level:
            opts.compaction_style = rdb::CompactionStyle::kCompactionStyleLevel;
            break;
        case RocksConfig::CompactionStyle::Universal:
            opts.compaction_style = rdb::CompactionStyle::kCompactionStyleUniversal;
            break;
        case RocksConfig::CompactionStyle::Fifo:
            opts.compaction_style = rdb::CompactionStyle::kCompactionStyleFIFO;
            break;
        }
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

namespace
{

void
reminder(RocksConfig::CompactionStyle) __attribute__((unused));

void
reminder(RocksConfig::CompactionStyle m)
{
    switch (m)
    {
    case RocksConfig::CompactionStyle::Level:
    case RocksConfig::CompactionStyle::Universal:
    case RocksConfig::CompactionStyle::Fifo:
        // If the compiler yells at you that you've forgotten dealing with an enum
        // value here chances are that it's also missing from the translations map
        // below. If so add it NOW.
        break;
    }
}

using TranslationsMap = boost::bimap<RocksConfig::CompactionStyle, std::string>;

TranslationsMap
init_translations()
{
    const std::vector<TranslationsMap::value_type> initv{
        { RocksConfig::CompactionStyle::Level, "Level" },
        { RocksConfig::CompactionStyle::Universal, "Universal" },
        { RocksConfig::CompactionStyle::Fifo, "Fifo" },
    };

    return TranslationsMap(initv.begin(),
                           initv.end());
}

}

std::ostream&
operator<<(std::ostream& os,
           const RocksConfig::CompactionStyle s)
{
    static const TranslationsMap translations(init_translations());
    return yt::StreamUtils::stream_out(translations.left,
                                       os,
                                       s);
}

std::istream&
operator>>(std::istream& is,
           RocksConfig::CompactionStyle& s)
{
    static const TranslationsMap translations(init_translations());
    return yt::StreamUtils::stream_in(translations.right,
                                      is,
                                      s);
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
        ",max_write_buffer_number=" << cfg.max_write_buffer_number <<
        ",min_write_buffer_number_to_merge=" << cfg.min_write_buffer_number_to_merge <<
        ",num_levels=" << cfg.num_levels <<
        ",level0_file_num_compaction_trigger=" << cfg.level0_file_num_compaction_trigger <<
        ",level0_slowdown_writes_trigger=" << cfg.level0_slowdown_writes_trigger <<
        ",level0_stop_writes_trigger=" << cfg.level0_stop_writes_trigger <<
        ",target_file_size_base=" << cfg.target_file_size_base <<
        ",max_bytes_for_level_base=" << cfg.max_bytes_for_level_base <<
        ",compaction_style=" << cfg.compaction_style <<
        "}";
}

}
