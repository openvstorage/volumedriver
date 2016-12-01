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

#include "CachedMetaDataStore.h"
#include "ClusterLocationAndHash.h"
#include "CombinedTLogReader.h"
#include "PageSortingGenerator.h"
#include "RelocationReaderFactory.h"
#include "TLog.h"
#include "TracePoints_tp.h"
#include "VolManager.h"
#include "VolumeConfig.h"

#include <boost/foreach.hpp>
#include <boost/scope_exit.hpp>

#include <youtils/Assert.h>
#include <youtils/ScopeExit.h>

namespace volumedriver
{

namespace be = backend;
namespace yt = youtils;

#define LOCK_CORKS_READ                                                 \
    boost::shared_lock<decltype(corks_lock_)> corksrg__(corks_lock_)

#define ASSERT_CORKS_READ_LOCKED                \
    ASSERT(not corks_lock_.try_lock())

#define LOCK_CORKS_WRITE                                                \
    boost::unique_lock<decltype(corks_lock_)> corksug__(corks_lock_)

#define ASSERT_CORKS_WRITE_LOCKED               \
    ASSERT(not corks_lock_.try_lock_shared())

#define LOCK_CACHE_READ                                                 \
    boost::shared_lock<decltype(cache_lock_)> cacherg__(cache_lock_)

#define ASSERT_CACHE_READ_LOCKED                \
    ASSERT(not cache_lock_.try_lock())

#define LOCK_CACHE_WRITE                                                \
    boost::unique_lock<decltype(cache_lock_)> cacheug__(cache_lock_)

#define ASSERT_CACHE_WRITE_LOCKED               \
    ASSERT(not cache_lock_.try_lock_shared());

#define LOCK_BACKEND                            \
    boost::lock_guard<decltype(backend_lock_)> backendg__(backend_lock_)

#define ASSERT_BACKEND_LOCKED                   \
    ASSERT(not backend_lock_.try_lock());

uint64_t CachedMetaDataStore::replayClustersCached = 8000000;

CachedMetaDataStore::CachedMetaDataStore(const MetaDataBackendInterfacePtr& backend,
                                         const std::string& id,
                                         uint64_t capacity)
    : backend_(backend)
    , page_data_(capacity * CachePage::capacity())
    , num_pages_(0)
    , cache_hits_(0)
    , cache_misses_(0)
    , written_clusters_(0)
    , discarded_clusters_(0)
    , id_(id)
{
    VERIFY(capacity > 0);

    {
        LOCK_BACKEND;
        cork_uuid_ = backend_->lastCorkUUID();
        scrub_id_ = backend_->scrub_id();
    }

    init_pages_(capacity);
}

CachedMetaDataStore::~CachedMetaDataStore()
{
    write_dirty_pages_to_backend_and_clear_page_list(true,
                                                     true);
}

void
CachedMetaDataStore::init_pages_(size_t capacity)
{
    pages_.reserve(capacity);

    for (uint64_t i = 0; i < capacity; ++i)
    {
        VERIFY((i + 1) * CachePage::capacity() <= page_data_.size());
        pages_.emplace_back(i, &page_data_[i * CachePage::capacity()]);
    }

    LOG_INFO(id_ <<
             ": page capacity (entries): " << CachePage::capacity() <<
             ", max cached pages: " << pages_.size());
}

void
CachedMetaDataStore::readCluster(const ClusterAddress caddr,
                                 ClusterLocationAndHash& loc)
{
    LOG_TRACE(id_ << ": ca " << caddr);
    {
        LOCK_CORKS_READ;
        // It seems that after a backend restart we *dont* have a current tlog?
        //        ASSERT(not corks_.empty());
        BOOST_REVERSE_FOREACH(const cork_t& crk, corks_)
        {
            ca_loc_map_type::const_iterator it = crk.second->find(caddr);
            if(it != crk.second->end())
            {
                loc = it->second;
                cache_hits_++;
                LOG_TRACE(id_ << ": ca " << caddr << " -> loc: " << loc);
                return;
            }
        }
    }

    get_cluster_location_(caddr, loc, false);

    if (ClusterLocationAndHash::use_hash() and loc.clusterLocation.isNull())
    {
        // - loc previously not written to: all 0's (loc 0, hash 0)
        // - loc discarded: loc 0, hash of zeroed page
        // -> Outside the MetaDataStore there should only be one representation
        //    (the one with the correct hash).
        // XXX: any other place missing this treatment?
        LOG_TRACE(id_ << ": ca " << caddr << " is null - fixing up hash");
        loc = ClusterLocationAndHash::discarded_location_and_hash();
    }

    LOG_TRACE(id_ << ": ca " << caddr << " -> loc: " << loc);
}

// must not be called concurrently by consumers.
void
CachedMetaDataStore::writeCluster(const ClusterAddress caddr,
                                  const ClusterLocationAndHash& loc)
{
    LOG_TRACE(id_ << ": ca " << caddr << ", loc " << loc);

    //changed to read for testing, needs revision
    LOCK_CORKS_READ;
    ASSERT(not corks_.empty());
    (*corks_.back().second)[caddr] = loc;
}

void
CachedMetaDataStore::discardCluster(const ClusterAddress caddr)
{
    LOG_TRACE(id_ << ": ca " << caddr);
    writeCluster(caddr, ClusterLocationAndHash::discarded_location_and_hash());
}

void
CachedMetaDataStore::clear_all_keys()
{
    LOCK_CORKS_WRITE;

    corks_.clear();
    write_dirty_pages_to_backend_and_clear_page_list(false, false);

    LOCK_BACKEND;

    backend_->clear_all_keys();
    cork_uuid_ = boost::none;
    scrub_id_ = boost::none;
}

bool
CachedMetaDataStore::compare(MetaDataStoreInterface& /*other*/)
{
    {
        LOCK_CORKS_READ;
        VERIFY(corks_.empty());
    }
    return false;
}

MetaDataStoreFunctor&
CachedMetaDataStore::for_each(MetaDataStoreFunctor& f,
                              const ClusterAddress max_address)
{
    // AR: LOCK_CORKS_READ; LOCK_CACHE_READ; instead for the full scope of for_each?
    {
        LOCK_CORKS_READ;
        VERIFY(corks_.size() == 1);
        VERIFY(corks_.front().second->empty());
        // VERIFY(page_list_.size() == 0);
    }

    LOCK_BACKEND;
    return backend_->for_each(f,
                              max_address);
}

void
CachedMetaDataStore::getStats(MetaDataStoreStats& stats)
{
    const int64_t used =
        written_clusters_ - discarded_clusters_ + backend_->getUsedClusters();
    ASSERT(used >= 0);

    stats.used_clusters = used;
    stats.cache_hits = cache_hits_;
    stats.cache_misses = cache_misses_;
    stats.cached_pages = num_pages_;
    stats.max_pages = pages_.size();
    stats.corked_clusters.clear();

    getCorkedClusters(stats.corked_clusters);
}

MaybeScrubId
CachedMetaDataStore::scrub_id()
{
    LOCK_CACHE_READ;
    return scrub_id_;
}

void
CachedMetaDataStore::set_scrub_id(const ScrubId& id)
{
    LOCK_CACHE_WRITE;
    LOCK_BACKEND;

    backend_->set_scrub_id(id);
    scrub_id_ = id;
}

boost::optional<yt::UUID>
CachedMetaDataStore::lastCork()
{
    LOCK_CORKS_READ;
    return cork_uuid_;
}

void
CachedMetaDataStore::cork(const yt::UUID& cork)
{
    tracepoint(openvstorage_volumedriver,
               mdstore_cork,
               id_.c_str(),
               cork.data());

    LOCK_CORKS_WRITE;
    LOG_INFO(id_ << ": corking " << cork);

    if(not corks_.empty() and
       corks_.back().first == cork)
    {
        LOG_INFO(id_ << ": recorking " << cork);
    }
    else
    {
        corks_.push_back(std::make_pair(cork,
                                        ca_loc_map_ptr_type(new ca_loc_map_type())));
    }
}

TODO("AR: check locking unCork vs. processCloneTLogs. Probably only safe b/c the latter is only run on restart?");

void
CachedMetaDataStore::unCork(const boost::optional<yt::UUID>& cork)
{
    LOG_INFO(id_ << ": uncorking " << cork);

    tracepoint(openvstorage_volumedriver,
               mdstore_uncork_start,
               id_.c_str(),
               cork ? cork->data() : yt::UUID::NullUUID().data());

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         tracepoint(openvstorage_volumedriver,
                                                    mdstore_uncork_end,
                                                    id_.c_str(),
                                                    cork ?
                                                    cork->data() :
                                                    yt::UUID::NullUUID().data(),
                                                    std::uncaught_exception());
                                     }));

    // Uncorking must not happen concurrently - currently this is guaranteed by
    // callers (TLog write tasks are barriers).
#ifndef NDEBUG
    ASSERT(uncork_dbg_mutex_.try_lock());
    BOOST_SCOPE_EXIT_TPL((&uncork_dbg_mutex_))
    {
        uncork_dbg_mutex_.unlock();
    }
    BOOST_SCOPE_EXIT_END;
#endif
    {
        if(cork != boost::none)
        {
            LOCK_CORKS_READ;
            VERIFY(not corks_.empty());
            VERIFY(corks_.front().first == *cork);
        }

        uint32_t misses = 0;

        // cork() could modify the corks_ list at the same time, so we'd technically
        // be in for undefined behaviour when reading at the same time without locking.
        ca_loc_map_ptr_type m;
        {
            LOCK_CORKS_READ;

            // If corks_.front() == corks_.back() writeCluster inserts into the map
            // we're iterating over - a recipe for desaster.
            // It *currently* cannot happen as we cork on TLog creation and uncork in
            // the TLog-written-to-backend callback.
            VERIFY(corks_.front() != corks_.back());

            m = corks_.front().second;
        }

        for (const auto& val : *m)
        {
            LOCK_CORKS_READ;
            // Y42 not correct, just write to the own cache
            // Ordening here might be better
            if (not get_cluster_location_(val.first, const_cast<ClusterLocationAndHash&>(val.second), true))
            {
                misses++;
            }
        }
        LOG_INFO(id_ << ": written " << corks_.front().second->size() <<
                 " entries to pages, " << misses << " cache misses");
    }

    {
        uint32_t dirty_count = 0;

        LOCK_CACHE_READ;
        for (CachePage& p : page_list_)
        {
            if (maybeWritePage_locked_context(p, false))
            {
                dirty_count++;
            }
        }
        LOG_INFO(id_ << ": written out " << dirty_count << " dirty pages");
    }

    LOCK_CORKS_WRITE;

    {
        LOCK_BACKEND;
        backend_->setCork(corks_.front().first);
    }

    cork_uuid_ = corks_.front().first;
    corks_.pop_front();

    LOG_INFO(id_ << ": finished uncorking " << cork);
}

void
CachedMetaDataStore::processCloneTLogs(const CloneTLogs& ctl,
                                       const NSIDMap& nsidmap,
                                       const fs::path& tlog_path,
                                       bool sync,
                                       const boost::optional<yt::UUID>& cork)
{
    LOCK_CORKS_WRITE;

    VERIFY(corks_.size() == 0 or
           (corks_.size() == 1 and
            corks_.front().second->empty()));

    SCOCloneID prev_cloneid;

    for (CloneTLogs::size_type i = 0; i < ctl.size(); ++i)
    {
        const SCOCloneID cloneid = ctl[i].first;
        if (i != 0)
        {
            VERIFY(prev_cloneid > cloneid);
        }
        prev_cloneid = cloneid;

        const OrderedTLogIds& tlogs = ctl[i].second;

        std::shared_ptr<TLogReaderInterface>
            r(CombinedTLogReader::create(tlog_path,
                                         tlogs,
                                         nsidmap.get(cloneid)->clone()));
        processTLogReaderInterface(r, cloneid);
    }

    if(sync)
    {
        LOCK_CACHE_WRITE;

        for (CachePage& p : page_list_)
        {
            maybeWritePage_locked_context(p, false);
        }

        if (cork != boost::none)
        {
            LOCK_BACKEND;
            backend_->setCork(*cork);
            backend_->sync();
        }
    }

    cork_uuid_ = cork;
}

uint64_t
CachedMetaDataStore::processPages(std::unique_ptr<yt::Generator<PageDataPtr>> r,
                                  SCOCloneID cloneid)
{
    uint64_t pages = 0;
    uint64_t entries = 0;

    LOG_INFO(id_ << ": starting processing pages for cloneID " <<
             static_cast<int>(cloneid));

    while(not r->finished())
    {
        PageData& m = *(r->current());
        for (const Entry& e : m)
        {
            ClusterLocationAndHash loc = e.clusterLocationAndHash();
            loc.clusterLocation.cloneID(cloneid);
            get_cluster_location_(e.clusterAddress(),
                                  const_cast<ClusterLocationAndHash&>(loc),
                                  true);
            entries++;
        }
        pages++;
        r->next();
    }

    LOG_INFO(id_ << ": finished. " << entries << " entries written in " << pages <<
             " pages.");
    return pages;
}

uint64_t
CachedMetaDataStore::processTLogReaderInterface(std::shared_ptr<TLogReaderInterface> r,
                                                SCOCloneID cloneid)
{
    std::unique_ptr<PageSortingGenerator_>
        psg(new PageSortingGenerator_(replayClustersCached, r));

    std::unique_ptr<yt::Generator<PageDataPtr>>
        g(new yt::ThreadedGenerator<PageDataPtr>(std::move(psg),
                                                 replayPagesQueued));

    return processPages(std::move(g), cloneid);
}

void
CachedMetaDataStore::write_dirty_pages_to_backend_and_clear_page_list(bool sync,
                                                                      bool ignore_errors)
{
    LOCK_CACHE_WRITE;
    do_write_dirty_pages_to_backend_and_clear_page_list(sync,
                                                        ignore_errors);
}

void
CachedMetaDataStore::do_write_dirty_pages_to_backend_and_clear_page_list(bool sync,
                                                                         bool ignore_errors)
{
    while (not page_list_.empty())
    {
        CachePage& p(page_list_.front());

        p.unlink_from_list();
        p.unlink_from_set();
        --num_pages_;

        if (sync)
        {
            maybeWritePage_locked_context(p, ignore_errors);
        }
    }

    ASSERT(page_map_.empty());
    ASSERT(num_pages_ == 0);

    for (auto& p : pages_)
    {
        p.reset();
    }
}

void
CachedMetaDataStore::write_dirty_pages_to_backend_keeping_page_list()
{
    LOCK_CACHE_WRITE;

    for (auto& page : page_list_)
    {
        if(page.dirty and not page.empty())
        {
            maybeWritePage_locked_context(page, false);
            LOCK_BACKEND;
            dispose_page(&MetaDataBackendInterface::putPage, page);
            page.dirty = false;
        }
    }
}

namespace
{

struct PageCmp
{
    bool
    operator()(const PageAddress& pa,
               const CachePage& cp) const
    {
        return pa < cp.page_address();
    }

    bool
    operator()(const CachePage& cp,
               const PageAddress& pa) const
    {
        return cp.page_address() < pa;
    }
};

}

uint64_t
CachedMetaDataStore::applyRelocs(RelocationReaderFactory& factory,
                                 SCOCloneID scid,
                                 const ScrubId& scrub_id)
{
    uint64_t relocNum = 0;
    const Entry* e_old = 0;

    // set a temporary scrub id - in case of a crash while applying the relocs this
    // will lead to the restart code throwing away the mdstore and starting from scratch.
    set_scrub_id(ScrubId());

    std::unique_ptr<TLogReaderInterface> treader(factory.get_one());

    while ((e_old = treader->nextLocation()))
    {
        //  No problem to keep the lock in the loop

        const Entry* e_new = treader->nextLocation();
        if(not e_new)
        {
            LOG_ERROR(id_ << ": wrong Relocation TLog, uneven number of entries");
            throw fungi::IOException("Wrong Relocation TLog, uneven number of entries",
                                     id_.c_str());
        }

        const ClusterAddress a_old = e_old->clusterAddress();
        if(a_old != e_new->clusterAddress())
        {
            LOG_ERROR(id_ << ": wrong Relocation TLog, old clusteraddress does not equal new clusteraddress");
            throw fungi::IOException("Wrong Relocation TLog, old clusteraddress does not equal new clusteraddress",
                                     id_.c_str());
        }

        ClusterLocation l_old = e_old->clusterLocation();

        l_old.cloneID(scid);

        ClusterLocationAndHash l_current;

        LOCK_CACHE_WRITE;

        get_cluster_location_unlocked_(a_old,
                                       l_current,
                                       false);

        if (l_current.clusterLocation == l_old)
        {
            ClusterLocationAndHash l_new = e_new->clusterLocationAndHash();
            l_new.clusterLocation.cloneID(scid);
            get_cluster_location_unlocked_(a_old,
                                           const_cast<ClusterLocationAndHash&>(l_new),
                                           true);
        }
        relocNum++;
    }

    write_dirty_pages_to_backend_keeping_page_list();
    set_scrub_id(scrub_id);

    sync();

    return relocNum;
}

bool
CachedMetaDataStore::get_cluster_location_(const ClusterAddress ca,
                               ClusterLocationAndHash& loc,
                               bool for_write)
{
    LOCK_CACHE_WRITE;

    return get_cluster_location_unlocked_(ca,
                                          loc,
                                          for_write);
}

void
CachedMetaDataStore::set_cache_capacity(const size_t new_capacity)
{
    LOG_INFO(id_ << ": request to change cache capacity from " <<
             pages_.size() << " to " << new_capacity);

    VERIFY(new_capacity > 0);

    LOCK_CORKS_WRITE;
    LOCK_CACHE_WRITE;

    if (new_capacity != pages_.size())
    {
        do_write_dirty_pages_to_backend_and_clear_page_list(true,
                                                            true);

        page_data_.resize(new_capacity * CachePage::capacity());
        pages_.clear();

        init_pages_(new_capacity);
    }
}

std::pair<CachePage*, bool>
CachedMetaDataStore::get_page_(const ClusterAddress ca)
{
    const PageAddress pa = CachePage::pageAddress(ca);
    bool hit = false;

    CachePage* page;
    auto it = page_map_.find(pa,
                             PageCmp());
    if (it == page_map_.end())
    {
        ++cache_misses_;

        if (num_pages_ < pages_.size())
        {
            page = &pages_[num_pages_];
        }
        else
        {
            page = &page_list_.front();
            page->unlink_from_list();
            page->unlink_from_set();
            --num_pages_;
            maybeWritePage_locked_context(*page, false);
        }

        ASSERT(not page->dirty);
        ASSERT(not page->is_in_set());
        ASSERT(not page->is_in_list());
        ASSERT(num_pages_ < pages_.size());

        page = new(page) CachePage(pa, page->data());

        const bool found = backend_->getPage(*page);
        if (not found)
        {
            page->reset();
        }

        page_map_.insert(*page);
        ++num_pages_;
    }
    else
    {
        ++cache_hits_;
        hit = true;
        page = &(*it);
        page->unlink_from_list();
    }

    ASSERT(not page->is_in_list());
    ASSERT(page->is_in_set());

    page_list_.push_back(*page);

    return std::make_pair(page, hit);
}

std::vector<ClusterLocationAndHash>
CachedMetaDataStore::get_page(const ClusterAddress ca)
{
    std::vector<ClusterLocationAndHash> vec(CachePage::capacity());

    {
        LOCK_CORKS_READ;
        LOCK_CACHE_WRITE;

        CachePage* page;
        std::tie(page, std::ignore) = get_page_(ca);

        VERIFY(page);
        memcpy(vec.data(), page->data(), vec.size());

        const ClusterAddress ca_start =
            CachePage::clusterAddress(CachePage::pageAddress(ca));
        const ClusterAddress ca_end = ca_start + CachePage::capacity();

        for (const auto& cork : corks_)
        {
            for (const auto& p : *cork.second)
            {
                if (p.first >= ca_start and p.first < ca_end)
                {
                    vec[CachePage::offset(p.first)] = p.second;
                }
            }
        }
    }

    if (ClusterLocationAndHash::use_hash())
    {
        for (auto& clh : vec)
        {
            if (static_cast<ClusterLocation>(clh).isNull())
            {
                clh = ClusterLocationAndHash::discarded_location_and_hash();
            }
        }
    }

    return vec;
}

bool
CachedMetaDataStore::get_cluster_location_unlocked_(const ClusterAddress ca,
                                                    ClusterLocationAndHash& loc,
                                                    bool for_write)
{
    bool hit;
    CachePage* page;
    std::tie(page, hit) = get_page_(ca);
    ASSERT(page);

    if (for_write)
    {
        ClusterLocationAndHash& clh = (*page)[CachePage::offset(ca)];
        if (clh.clusterLocation.isNull() and
            not loc.clusterLocation.isNull())
        {
            page->written_clusters_since_last_backend_write++;
            written_clusters_++;
        }
        else if (not clh.clusterLocation.isNull() and
                 loc.clusterLocation.isNull())
        {
            page->discarded_clusters_since_last_backend_write++;
            discarded_clusters_++;
        }

        clh = loc;
        page->dirty = true;
    }
    else
    {
        loc = (*page)[CachePage::offset(ca)];
    }

    return hit;
}

void
CachedMetaDataStore::dispose_page(backend_mem_fun dispose, CachePage& p)
{
    ASSERT_BACKEND_LOCKED;

    const int32_t delta = p.written_clusters_since_last_backend_write -
        p.discarded_clusters_since_last_backend_write;

    ((*backend_).*dispose)(p, delta);

    written_clusters_ -= p.written_clusters_since_last_backend_write;
    discarded_clusters_ -= p.discarded_clusters_since_last_backend_write;
    p.written_clusters_since_last_backend_write = 0;
    p.discarded_clusters_since_last_backend_write = 0;
    p.dirty = false;
}

// TODO: rather expensive - can we be more clever?
bool
CachedMetaDataStore::possibly_discard_page_(CachePage& p)
{
    ASSERT_CACHE_READ_LOCKED;
    ASSERT(p.dirty);

    LOCK_BACKEND;

    if (backend_->pageExistsInParent(p.page_address()))
    {
        return false;
    }
    else
    {
        dispose_page(&MetaDataBackendInterface::discardPage, p);
        return true;
    }
}

bool
CachedMetaDataStore::maybeWritePage_locked_context(CachePage& p,
                                                   bool ignore_errors)
{
    //CACHE_READ_LOCK should be sufficient:
    //other readers don't change the dirtyness, discarded_clusters_, written_clusters_

    ASSERT_CACHE_READ_LOCKED;
    if (p.dirty)
    {
        try
        {
            if (p.empty())
            {
                auto discarded = possibly_discard_page_(p);
                if (discarded)
                {
                    return false;
                }
            }

            LOCK_BACKEND;
            dispose_page(&MetaDataBackendInterface::putPage, p);
            return true;
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR(id_ << ": failed to write page with PageAddress " <<
                          p.page_address() << EWHAT);
                if (not ignore_errors)
                {
                    throw;
                }
                else
                {
                    return false;
                }
            });
    }
    else
    {
        return false;
    }
}

bool
CachedMetaDataStore::freezeable() const
{
    return backend_->freezeable();
}

void
CachedMetaDataStore::set_delete_local_artefacts_on_destroy() noexcept
{
    backend_->set_delete_local_artefacts_on_destroy();
}

void
CachedMetaDataStore::set_delete_global_artefacts_on_destroy() noexcept
{
    backend_->set_delete_global_artefacts_on_destroy();
}

#ifndef NDEBUG
void
CachedMetaDataStore::_print_corks_()
{
    LOCK_CORKS_READ;

    std::cout << corks_.size() << " corks in CachedMetaDataStore" << std::endl;
    for (const auto& cork : corks_)
    {
        std::cout << "Cork " << cork.first << " has " << cork.second->size() <<
            " entries." << std::endl;
    }

    std::cout << "Current cork here is " << cork_uuid_ << ", on the backend it's " <<
        backend_->lastCorkUUID();
}
#endif

void
CachedMetaDataStore::getCorkedClusters(std::vector<std::pair<yt::UUID,
                                                   uint64_t>>& vec) const
{
    LOCK_CORKS_READ;

    for (const auto& cork : corks_)
    {
        vec.emplace_back(cork.first, cork.second->size());
    }
}

void
CachedMetaDataStore::copy_corked_entries_from(const CachedMetaDataStore& other)
{
    boost::shared_lock<decltype(other.corks_lock_)> g(other.corks_lock_);

    LOCK_CORKS_WRITE;
    corks_ = other.corks_;
}

void
CachedMetaDataStore::updateBackendConfig(const MetaDataBackendConfig&)
{
    LOG_ERROR("Updating the MetaDataBackendConfig is not supported");
    throw UpdateMetaDataBackendConfigException("Updating the backend configuration is not supported by CachedMetaDataStores");
}

std::unique_ptr<MetaDataBackendConfig>
CachedMetaDataStore::getBackendConfig() const
{
    LOCK_BACKEND; // not strictly necessary.
    return backend_->getConfig();
}

}

// Local Variables: **
// mode: c++ **
// End: **
