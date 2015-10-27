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

#include "MetaDataBackendInterface.h"
#include "RocksDBMetaDataBackend.h"
#include "VolManager.h"

#include <rocksdb/comparator.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/status.h>

#include <youtils/RocksLogger.h>

namespace volumedriver
{

namespace rdb = rocksdb;
namespace yt = youtils;

#define HANDLE(s)                                                       \
    if (not s.ok())                                                     \
    {                                                                   \
        LOG_ERROR("quoth rocksdb: " << s.ToString());                   \
        throw MetaDataStoreBackendException(s.ToString().c_str(),       \
                                            __FUNCTION__);              \
    }

const uint64_t
RocksDBMetaDataBackend::cork_key_ = std::numeric_limits<uint64_t>::max();

const uint64_t
RocksDBMetaDataBackend::used_clusters_key_ = std::numeric_limits<uint64_t>::max() - 1;

const uint64_t
RocksDBMetaDataBackend::scrub_id_key_ = std::numeric_limits<uint64_t>::max() - 2;

const std::string
RocksDBMetaDataBackend::db_name = "mdstore.rocksdb";

namespace
{

class Comparator
    : public rdb::Comparator
{
public:
    Comparator()
        : rdb::Comparator()
    {}

    virtual int
    Compare(const rdb::Slice& a,
            const rdb::Slice& b) const
    {
        const PageAddress paa = *reinterpret_cast<const PageAddress*>(a.data_);
        const PageAddress pbb = *reinterpret_cast<const PageAddress*>(b.data_);

        if (paa < pbb)
        {
            return -1;
        }
        else if (paa > pbb)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    virtual const char*
    Name() const
    {
        return "PageAddressComparator";
    }

    virtual void
    FindShortestSeparator(std::string* /* start */,
                          const rdb::Slice& /* limit */) const
    {}

    virtual void
    FindShortSuccessor(std::string* /* key */) const
    {}
};

const Comparator page_address_comparator;

rdb::Options
make_db_options(const std::string& id)
{
    rdb::Options opts;

    opts.OptimizeLevelStyleCompaction();
    // opts.OptimizeForPointLookup(); // we don't need iterators atm
    opts.write_buffer_size = 32ULL << 20; // default: 4 MiB
    // opts.block_size = CachePage::size();
    // default: kSnappyCompression, which is claimed to be fast enough
    opts.compression = rdb::CompressionType::kNoCompression;
    opts.verify_checksums_in_compaction = false;
    // that seems not to improve things compared to the default comparator
    // opts.comparator = &page_address_comparator;

    // opts.IncreaseParallelism(::sysconf(_SC_NPROCESSORS_ONLN));
    opts.create_if_missing = true;
    opts.disableDataSync = true;
    opts.paranoid_checks = false;
    opts.info_log = std::make_shared<yt::RocksLogger>(id);

    return opts;
}

rdb::ReadOptions
make_read_options()
{
    rdb::ReadOptions opts;
    opts.verify_checksums = false;
    return opts;
}

rdb::WriteOptions
make_write_options()
{
    rdb::WriteOptions opts;
    opts.disableWAL = true;
    return opts;
}

}

RocksDBMetaDataBackend::RocksDBMetaDataBackend(const VolumeConfig& cfg,
                                               bool writer)
    : used_clusters_(0)
    , delete_local_artefacts(DeleteLocalArtefacts::F)
    , delete_global_artefacts(DeleteGlobalArtefacts::F)
    , nspace_(cfg.ns_)
    , filename_(VolManager::get()->getMetaDataPath(nspace_) / db_name)
{
    fs::create_directories(VolManager::get()->getMetaDataPath(nspace_));

    const rdb::Options opts(make_db_options(nspace_.str()));
    rdb::DB* db;

    if (writer)
    {
        HANDLE(rdb::DB::Open(opts,
                             filename_.string(),
                             &db));
    }
    else
    {
        HANDLE(rdb::DB::OpenForReadOnly(opts,
                                        filename_.string(),
                                        &db));
    }

    db_.reset(db);
    VERIFY(db != nullptr);

    std::string res;
    const rdb::Status status(db_->Get(make_read_options(),
                                      rdb::Slice(reinterpret_cast<const char*>(&used_clusters_key_),
                                                 sizeof(used_clusters_key_)),
                                      &res));
    switch (status.code())
    {
    case rdb::Status::kOk:
        {
            ASSERT(res.size() == sizeof(used_clusters_));
            used_clusters_ = *reinterpret_cast<const decltype(used_clusters_)*>(res.data());
            break;
        }
    case rdb::Status::kNotFound:
        {
            break;
        }
    default:
        {
            HANDLE(status);
        }
    }

    LOG_INFO(cfg.id_ << ": metadata backend ready");
}

RocksDBMetaDataBackend::~RocksDBMetaDataBackend()
{
    if (T(delete_local_artefacts))
    {
        try
        {
            fs::remove_all(VolManager::get()->getMetaDataPath(nspace_));
        }
        CATCH_STD_ALL_LOG_IGNORE("Could not delete MDStore as requested");
    }
}

bool
RocksDBMetaDataBackend::getPage(CachePage& p)
{
    const PageAddress pa = p.page_address();

    LOG_TRACE("page address " << pa);

    check_page_address_(pa);

    std::string res;
    const rdb::Status status(db_->Get(make_read_options(),
                                      rdb::Slice(reinterpret_cast<const char*>(&pa),
                                                 sizeof(pa)),
                                      &res));
    switch (status.code())
    {
    case rdb::Status::kOk:
        {
            VERIFY(res.size() == CachePage::size());
            memcpy(p.data(), res.data(), res.size());
            return true;
        }
    case rdb::Status::kNotFound:
        {
            return false;
        }
    default:
        {
            HANDLE(status);
        }
    }

    UNREACHABLE;
}

void
RocksDBMetaDataBackend::putPage(const CachePage& p,
                                int32_t used_clusters_diff)
{
    const PageAddress pa = p.page_address();

    LOG_TRACE("page address " << pa << ", used_clusters_delta " <<
              used_clusters_diff);

    check_page_address_(pa);

    const int64_t used_clusters = used_clusters_ + used_clusters_diff;
    ASSERT(used_clusters >= 0);

    HANDLE(db_->Put(make_write_options(),
                    rdb::Slice(reinterpret_cast<const char*>(&pa),
                               sizeof(pa)),
                    rdb::Slice(reinterpret_cast<const char*>(p.data()),
                               p.size())));

    HANDLE(db_->Put(make_write_options(),
                    rdb::Slice(reinterpret_cast<const char*>(&used_clusters_key_),
                               sizeof(used_clusters_key_)),
                    rdb::Slice(reinterpret_cast<const char*>(&used_clusters),
                               sizeof(used_clusters))));
}

void
RocksDBMetaDataBackend::discardPage(const CachePage& p,
                                    int32_t used_clusters_diff)
{
    const PageAddress pa = p.page_address();

    LOG_TRACE("page address " << pa << ", used_clusters_delta " <<
              used_clusters_diff);

    check_page_address_(pa);

    const int64_t used_clusters = used_clusters_ + used_clusters_diff;
    ASSERT(used_clusters >= 0);

    HANDLE(db_->Delete(make_write_options(),
                       rdb::Slice(reinterpret_cast<const char*>(&pa),
                                  sizeof(pa))));

    HANDLE(db_->Put(make_write_options(),
                    rdb::Slice(reinterpret_cast<const char*>(&used_clusters_key_),
                               sizeof(used_clusters_key_)),
                    rdb::Slice(reinterpret_cast<const char*>(&used_clusters),
                               sizeof(used_clusters))));
}

void
RocksDBMetaDataBackend::sync()
{
    HANDLE(db_->Flush(rdb::FlushOptions()));
}

void
RocksDBMetaDataBackend::clear_all_keys()
{
    db_.reset();

    const rdb::Options opts(make_db_options(nspace_.str()));
    rdb::DB* db;

    HANDLE(rdb::DestroyDB(filename_.string(),
                          opts));

    HANDLE(rdb::DB::Open(opts,
                         filename_.string(),
                         &db));

    db_.reset(db);
}

MetaDataStoreFunctor&
RocksDBMetaDataBackend::for_each(MetaDataStoreFunctor& f,
                                 const ClusterAddress ca_max)
{
    // Same implementation as Arakoon's (minus the queue flushing) ... move up?
    const size_t page_size = CachePage::capacity();
    std::vector<ClusterLocationAndHash> clh(page_size);

    for (ClusterAddress ca = 0; ca < ca_max; ca += page_size)
    {
        PageAddress pa = CachePage::pageAddress(ca);
        check_page_address_(pa);

        CachePage p(pa, clh.data());
        bool found = getPage(p);
        if (found)
        {
            for (size_t i = 0; i < page_size; ++i)
            {
                ClusterLocationAndHash loc(p[i]);
                if (not loc.clusterLocation.isNull())
                {
                    f(CachePage::clusterAddress(pa) + i, loc);
                }
            }
        }
    }

    return f;
}

void
RocksDBMetaDataBackend::setCork(const youtils::UUID& cork_uuid)
{
    LOG_TRACE(cork_uuid);

    HANDLE(db_->Put(make_write_options(),
                    rdb::Slice(reinterpret_cast<const char*>(&cork_key_),
                               sizeof(cork_key_)),
                    rdb::Slice(cork_uuid.str())));
}

boost::optional<youtils::UUID>
RocksDBMetaDataBackend::lastCorkUUID()
{
    std::string res;
    const rdb::Status status(db_->Get(make_read_options(),
                                      rdb::Slice(reinterpret_cast<const char*>(&cork_key_),
                                                 sizeof(cork_key_)),
                                      &res));

    switch (status.code())
    {
    case rdb::Status::kOk:
        {
            ASSERT(res.size() == yt::UUID::getUUIDStringSize());
            LOG_TRACE("last cork UUID: " << res);
            return boost::optional<yt::UUID>(yt::UUID(res));
        }
    case rdb::Status::kNotFound:
        {
            LOG_TRACE("no last cork UUID");
            return boost::none;
        }
    default:
        {
            HANDLE(status);
        }
    }

    UNREACHABLE;
}

MaybeScrubId
RocksDBMetaDataBackend::scrub_id()
{
    std::string res;
    const rdb::Status status(db_->Get(make_read_options(),
                                      rdb::Slice(reinterpret_cast<const char*>(&scrub_id_key_),
                                                 sizeof(scrub_id_key_)),
                                      &res));

    switch (status.code())
    {
    case rdb::Status::kOk:
        {
            ASSERT(res.size() == yt::UUID::getUUIDStringSize());
            LOG_TRACE("scrub ID: " << res);
            return ScrubId(yt::UUID(res));
        }
    case rdb::Status::kNotFound:
        {
            LOG_TRACE("no scrub ID");
            return boost::none;
        }
    default:
        {
            HANDLE(status);
        }
    }

    UNREACHABLE;
}

void
RocksDBMetaDataBackend::set_scrub_id(const ScrubId& scrub_id)
{
    LOG_INFO("Setting scrub id " << scrub_id);

    HANDLE(db_->Put(make_write_options(),
                    rdb::Slice(reinterpret_cast<const char*>(&scrub_id_key_),
                               sizeof(scrub_id_key_)),
                    rdb::Slice(static_cast<const yt::UUID&>(scrub_id).str())));
}

uint64_t
RocksDBMetaDataBackend::locally_required_bytes_(const VolumeConfig& cfg)
{
    // Quoth the tokyocabinet(3) manpage:
    // "Moreover, the size of database of Tokyo Cabinet is very small. For example,
    //  overhead for a record is 16 bytes for hash database, and 5 bytes for B+ tree
    //  database."
    //
    // => 5 bytes overhead + Page::pageSize_ per value + 8 bytes per key.
    //
    // This does not account for global metadata, so take it with a pinch of salt.
    const uint64_t vsize = cfg.lba_size_ * cfg.lba_count();

#if 0
    const uint64_t csize = cfg.getClusterSize();

    VERIFY(csize > 0);

    const uint64_t nclusters = vsize / csize + (vsize % csize ? 1 : 0);
    const uint64_t pentries = Page::pageEntries_;
    VERIFY(pentries > 0);
    const uint64_t npages = nclusters / pentries + (nclusters % pentries ? 1 : 0);

    return npages * (5 + Page::pageSize_ + 8);
#endif

    //
    // Tests in a range from 16MiB ... 1TiB (test is performed actually reverse) show
    // a linear increase
    //
    //          [(1099511627776, 6450602072),
    //           (549755813888, 3225425992),
    //           (274877906944, 1612837912),
    //           (137438953472, 806543928),
    //           (68719476736, 403405048),
    //           (34359738368, 201835608),
    //           (17179869184, 101050888),
    //           (8589934592, 50658528),
    //           (4294967296, 25462352),
    //           (2147483648, 12864232),
    //           (1073741824, 6565224),
    //           (536870912, 3415776),
    //           (268435456, 1841056),
    //           (134217728, 1053696),
    //           (67108864, 660072),
    //           (33554432, 463264),
    //           (16777216, 364864)]
    //
    // y = m * x + n
    //
    // (1) 6450602072 = m * 1099511627776 + n
    // (2) 364864 = m * 16777216 + n
    //
    const double m = (6450602072.0 - 364864.0) / (1099511627776.0 - 16777216.0);
    const double n = 364864.0 - m * 16777216.0;

    return m * vsize + n;
}

uint64_t
RocksDBMetaDataBackend::locally_required_bytes(const VolumeConfig& cfg)
{
    const uint64_t estimate = locally_required_bytes_(cfg);
    const uint64_t used = locally_used_bytes(cfg);

    if (used > estimate)
    {
        LOG_WARN(cfg.id_ << ": real usage " << used << " > estimate " << estimate <<
                 " - fix the overhead calculation");
        return used;
    }
    else
    {
        return estimate;
    }
}

uint64_t
RocksDBMetaDataBackend::locally_used_bytes(const VolumeConfig& cfg)
{
    const fs::path p(VolManager::get()->getMetaDataPath(cfg));
    try
    {
        return fs::file_size(p / db_name);
    }
    CATCH_STD_ALL_LOGLEVEL_IGNORE("failed to determine size of " << p, WARN);

    return 0;
}

std::unique_ptr<MetaDataBackendConfig>
RocksDBMetaDataBackend::getConfig() const
{
    return std::unique_ptr<MetaDataBackendConfig>(new RocksDBMetaDataBackendConfig());
}

void
RocksDBMetaDataBackend::check_page_address_(const PageAddress pa)
{
    if (pa == cork_key_ or
        pa == used_clusters_key_ or
        pa == scrub_id_key_)
    {
        LOG_ERROR("Page address " << pa << " conflicts with internal key");
        throw MetaDataStoreBackendException("Page address conflicts with internal key");
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
