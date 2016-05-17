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

#include "MetaDataBackendInterface.h"
#include "TokyoCabinetMetaDataBackend.h"
#include "VolManager.h"

#include <tcutil.h>

namespace volumedriver
{

namespace yt = youtils;

#define HANDLE(x) if (not x)                                            \
    {                                                                   \
        std::string msg("problem with tc: ");                           \
        if (database_)                                                  \
        {                                                               \
            const int ecode = tcbdbecode(database_);                    \
            msg += tcbdberrmsg(ecode);                                  \
            LOG_ERROR(msg);                                             \
        }                                                               \
        throw MetaDataStoreBackendException(msg.c_str() ,__FUNCTION__); \
    }

const uint64_t
TokyoCabinetMetaDataBackend::cork_key_ = std::numeric_limits<uint64_t>::max();

const uint64_t
TokyoCabinetMetaDataBackend::used_clusters_key_ = std::numeric_limits<uint64_t>::max() - 1;

const uint64_t
TokyoCabinetMetaDataBackend::scrub_id_key_ = std::numeric_limits<uint64_t>::max() - 2;

const std::string
TokyoCabinetMetaDataBackend::db_name = "mdstore.tc";

TokyoCabinetMetaDataBackend::TokyoCabinetMetaDataBackend(const VolumeConfig& cfg,
                                                         bool writer)
    : TokyoCabinetMetaDataBackend(VolManager::get()->getMetaDataPath(cfg),
                                  writer)
{
    LOG_INFO(cfg.id_ << ": metadata backend ready");
}

TokyoCabinetMetaDataBackend::TokyoCabinetMetaDataBackend(const fs::path& db_directory,
                                                         bool writer)
    : used_clusters_(0)
    , database_(tcbdbnew())
    , filename_(db_directory / db_name)
    , delete_local_artefacts(DeleteLocalArtefacts::F)
    , delete_global_artefacts(DeleteGlobalArtefacts::F)
{
    fs::create_directories(db_directory);
    if (!database_)
    {
        LOG_ERROR("Failed to create tc database object");
        throw MetaDataStoreBackendException("failed to create tc database object");
    }

    try
    {
        HANDLE(tcbdbtune(database_,
                         // config ? config->numberOfMembersInLeafPage :
                         0 ,
                         // config ? config->numberOfMembersInNonLeafPage :
                         0,
                         // config ? config->numberOfElementsInBucketArray :
                         0,
                         // config ? config->recordAlignmentByPowerOfTwo:
                         3,
                         // config ? config->maximumNumberInFreeBlockByPowerOf          Two :
                         -1,
                         BDBTLARGE));
        HANDLE(tcbdbsetcache(database_,
                             // config ? config->maximumNumberOfLeafNodesCached :
                             0,
                             // config ? config->maximumNumberOfNonLeafNodesCached :
                             0));
        VERIFY(sizeof(ClusterAddress) == 8);
        if (not tcbdbsetcmpfunc(database_,tccmpint64,NULL))
        {
            LOG_ERROR("Could not set the compare function");
            throw MetaDataStoreBackendException("Could not set compare function");
        }
        int o_mode = (writer ? BDBOWRITER : 0) | BDBOCREAT | BDBOREADER;

        HANDLE(tcbdbopen(database_, filename_.string().c_str(), o_mode));
        tcfatalfunc = &fatalLog_;
    }
    catch (MetaDataStoreBackendException&)
    {
        tcbdbdel(database_);
        database_ = 0;
        throw;
    }
    int32_t used_clusters_size;

    void* ret = tcbdbget(database_,
                         &used_clusters_key_,
                         sizeof(used_clusters_key_),
                         &used_clusters_size);
    BOOST_SCOPE_EXIT((ret))
    {
        free(ret);
    }
    BOOST_SCOPE_EXIT_END;

    if (ret)
    {
        VERIFY(used_clusters_size == sizeof(used_clusters_));
        used_clusters_ = *reinterpret_cast<uint64_t*>(ret);
    }
}

TokyoCabinetMetaDataBackend::~TokyoCabinetMetaDataBackend()
{
    tcbdbclose(database_);
    tcbdbdel(database_);
    if (T(delete_local_artefacts))
    {
        try
        {
            fs::remove(filename_);
        }
        catch(std::exception& e)
        {
            LOG_ERROR("Could not delete MDStore as requested, " << e.what() << ", ignoring this");
        }
        catch(...)
        {
            LOG_ERROR("Could not delete MDStore as requested, unknown error, ignoring this");
        }
    }
}

bool
TokyoCabinetMetaDataBackend::getPage(CachePage& p)
{
    LOG_TRACE("page address " << p.page_address());

    check_page_address_(p);

    int32_t numbits;
    const void* ret = tcbdbget3(database_,
                                &p.page_address(),
                                sizeof(p.page_address()),
                                &numbits);
    if (ret)
    {
        VERIFY((uint32_t)numbits == CachePage::size());
        memcpy(p.data(), ret, numbits);
        return true;
    }
    else
    {
        return false;
    }
}

void
TokyoCabinetMetaDataBackend::putPage(const CachePage& p,
                                     int32_t used_clusters_diff)
{
    LOG_TRACE("page address " << p.page_address() << ", used_clusters_delta " <<
              used_clusters_diff);

    check_page_address_(p);

    const int64_t used_clusters = used_clusters_ + used_clusters_diff;
    ASSERT(used_clusters >= 0);

    HANDLE(tcbdbput(database_,
                    &p.page_address(),
                    sizeof(p.page_address()),
                    p.data(),
                    CachePage::size()));
    used_clusters_ = used_clusters;

    HANDLE(tcbdbput(database_,
                    &used_clusters_key_,
                    sizeof(used_clusters_key_),
                    &used_clusters_,
                    sizeof(used_clusters_)));
}

void
TokyoCabinetMetaDataBackend::discardPage(const CachePage& p,
                                         int32_t used_clusters_diff)
{
    LOG_TRACE("page address " << p.page_address() << ", used_clusters_delta " <<
              used_clusters_diff);

    check_page_address_(p);

    const int64_t used_clusters = used_clusters_ + used_clusters_diff;
    ASSERT(used_clusters >= 0);

    const bool res = tcbdbout(database_,
                              &p.page_address(),
                              sizeof(p.page_address()));
    if (not res)
    {
        const int ecode = tcbdbecode(database_);
        if (ecode != TCENOREC)
        {
            std::stringstream ss;
            ss << "problem with tc: failed to remove page " << p.page_address()
               << ": " << tcbdberrmsg(ecode);
            throw MetaDataStoreBackendException(ss.str().c_str(), __FUNCTION__);
        }
    }
    else
    {
        used_clusters_ = used_clusters;

        HANDLE(tcbdbput(database_,
                        &used_clusters_key_,
                        sizeof(used_clusters_key_),
                        &used_clusters_,
                        sizeof(used_clusters_)));
    }
}

void
TokyoCabinetMetaDataBackend::sync()
{
    HANDLE(tcbdbsync(database_));
}

void
TokyoCabinetMetaDataBackend::clear_all_keys()
{
    tcbdbvanish(database_);
}

MetaDataStoreFunctor&
TokyoCabinetMetaDataBackend::for_each(MetaDataStoreFunctor& f,
                                      const ClusterAddress ca_max)
{
    // Same implementation as Arakoon's (minus the queue flushing) ... move up?
    const size_t page_size = CachePage::capacity();
    std::vector<ClusterLocationAndHash> clh(page_size);

    for (ClusterAddress ca = 0; ca < ca_max; ca += page_size)
    {
        PageAddress pa = CachePage::pageAddress(ca);

        CachePage p(pa, clh.data());

        check_page_address_(p);

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
TokyoCabinetMetaDataBackend::setCork(const yt::UUID& cork_uuid)
{
    LOG_TRACE(cork_uuid);

    HANDLE(tcbdbput(database_,
                    &cork_key_,
                    sizeof(cork_key_),
                    cork_uuid.str().c_str(),
                    yt::UUID::getUUIDStringSize()));
}

boost::optional<yt::UUID>
TokyoCabinetMetaDataBackend::lastCorkUUID()
{
    int return_size;
    void* retval = tcbdbget(database_,
                            &cork_key_,
                            sizeof(cork_key_),
                            &return_size);
    BOOST_SCOPE_EXIT((retval))

    {
        free(retval);
    }
    BOOST_SCOPE_EXIT_END;

    if (retval)
    {
        VERIFY((size_t)return_size == yt::UUID::getUUIDStringSize());
        return boost::optional<yt::UUID>(yt::UUID(static_cast<char*>(retval)));
    }
    else
    {
        return boost::none;
    }
}

uint64_t
TokyoCabinetMetaDataBackend::locally_required_bytes_(const VolumeConfig& cfg)
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
TokyoCabinetMetaDataBackend::locally_required_bytes(const VolumeConfig& cfg)
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
TokyoCabinetMetaDataBackend::locally_used_bytes(const VolumeConfig& cfg)
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
TokyoCabinetMetaDataBackend::getConfig() const
{
    return std::unique_ptr<MetaDataBackendConfig>(new TCBTMetaDataBackendConfig());
}

void
TokyoCabinetMetaDataBackend::check_page_address_(const CachePage& p)
{
    const PageAddress pa = p.page_address();

    if (pa == cork_key_ or
        pa == used_clusters_key_ or
        pa == scrub_id_key_)
    {
        LOG_ERROR("Page address " << pa << " conflicts with internal key");
        throw MetaDataStoreBackendException("Page address conflicts with internal key");
    }
}

MaybeScrubId
TokyoCabinetMetaDataBackend::scrub_id()
{
    int return_size;
    void* retval = tcbdbget(database_,
                            &scrub_id_key_,
                            sizeof(scrub_id_key_),
                            &return_size);
    BOOST_SCOPE_EXIT((retval))
    {
        free(retval);
    }
    BOOST_SCOPE_EXIT_END;

    if (retval)
    {
        VERIFY((size_t)return_size == yt::UUID::getUUIDStringSize());
        return ScrubId(yt::UUID(static_cast<char*>(retval)));
    }
    else
    {
        return boost::none;
    }
}

void
TokyoCabinetMetaDataBackend::set_scrub_id(const ScrubId& scrub_id)
{
    LOG_INFO("Setting scrub ID " << scrub_id);

    HANDLE(tcbdbput(database_,
                    &scrub_id_key_,
                    sizeof(scrub_id_key_),
                    static_cast<const yt::UUID&>(scrub_id).str().c_str(),
                    yt::UUID::getUUIDStringSize()));
}

}

// Local Variables: **
// mode: c++ **
// End: **
