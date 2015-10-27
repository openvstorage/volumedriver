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

#include "ArakoonMetaDataBackend.h"
#include "CachedMetaDataPage.h"
#include "CachedMetaDataStore.h"
#include "MetaDataBackendConfig.h"
#include "VolumeConfig.h"
#include "VolManager.h" // <-- kill it!!!

#include <cstring>

#include <youtils/Catchers.h>

namespace volumedriver
{

namespace ara = arakoon;
namespace be = backend;
namespace yt = youtils;

class ArakoonMetaDataPageKey
{
    // these all have the same size... we should allocate them from a small heap
public:
    ArakoonMetaDataPageKey(const std::string& prefix,
                           const PageAddress& pa)
        : size_ (prefix.size() + sizeof(pa))
        , data_((char*)malloc(size_))
    {
        ASSERT(not prefix.empty());
        VERIFY(data_);
        memcpy(data_,
               prefix.c_str(),
               prefix.size());
        *reinterpret_cast<ClusterAddress*>(data_ + (size_ - sizeof(pa))) = pa;
    }

    ~ArakoonMetaDataPageKey()
    {
        free(data_);
    }

    ArakoonMetaDataPageKey(const ArakoonMetaDataPageKey&) = delete;
    ArakoonMetaDataPageKey& operator=(const ArakoonMetaDataPageKey*) = delete;

    const size_t size_;
    char* data_;

private:
    DECLARE_LOGGER("ArakoonMetaDataPageKey");
};

}

namespace arakoon
{

template<>
struct DataBufferTraits<volumedriver::CachePage>
{
    static size_t
    size(const volumedriver::CachePage&)
    {
        return volumedriver::CachePage::size();
    }

    static const void*
    data(const volumedriver::CachePage& page)
    {
        return (const void*)page.data();
    }
};

template<>
struct DataBufferTraits<volumedriver::ArakoonMetaDataPageKey>
{
    static size_t
    size(const volumedriver::ArakoonMetaDataPageKey& key)
    {
        return key.size_;
    }

    static const void*
    data(const volumedriver::ArakoonMetaDataPageKey& key)
    {
        return key.data_;
    }
};

template<>
struct DataBufferTraits<int64_t>
{
    static size_t
    size(const int64_t&)
    {
        return sizeof(uint64_t);
    }

    static const void*
    data(const int64_t& key)
    {
        return &key;
    }
};

}

namespace volumedriver
{

const std::string
ArakoonMetaDataBackend::separator_("/");

const std::string
ArakoonMetaDataBackend::volume_prefix_("/volumedriver/");

const std::string
ArakoonMetaDataBackend::cork_suffix_ = "corrukuud";

const std::string
ArakoonMetaDataBackend::scrub_id_suffix_ = "scrub_id";

const std::string
ArakoonMetaDataBackend::used_clusters_suffix_ = "used_clusters";

const std::string
ArakoonMetaDataBackend::emancipated_suffix_ = "emancipated_clone";

const std::string
ArakoonMetaDataBackend::page_dir_name_("pages/");

const uint32_t
ArakoonMetaDataBackend::timeout_ms_ = 60000;

ArakoonMetaDataBackend::ArakoonMetaDataBackend(const ArakoonMetaDataBackendConfig& cfg,
                                               const be::Namespace& nspace)
    : ns_(nspace)
    , cluster_(cfg.cluster_id(),
               cfg.node_configs(),
               ara::Cluster::MilliSeconds(timeout_ms_))
    , parent_ns_(
#ifdef CHEAP_CLONING
#warning "AR: push this to the MetaDataBackendConfig?"
                 cfg.clone_from_a_frozen_volume_data_structure_ ?
                 boost::optional<std::string>(cfg.parent_ns_) :
#endif
                 boost::none)
    , page_dir_(get_key(page_dir_name_, ns_))
    , parent_page_dir_(parent_ns_ ? get_key(page_dir_name_, *parent_ns_) : "")
    , cork_key_(get_key(cork_suffix_, ns_))
    , scrub_id_key_(get_key(scrub_id_suffix_, ns_))
    , used_clusters_key_(get_key(used_clusters_suffix_, ns_))
    , emancipated_key_(get_key(emancipated_suffix_, ns_))
    , used_clusters_(0)
    , write_sequence_data_(VolManager::get()->arakoon_metadata_sequence_size.value() *
                           CachePage::capacity())
    , config_(cfg)
{
    write_sequence_.reserve(VolManager::get()->arakoon_metadata_sequence_size.value());

    try
    {
        ara::buffer buf = cluster_.get(used_clusters_key_);
        ASSERT(buf.size() == sizeof(uint64_t));
        used_clusters_ = *reinterpret_cast<const uint64_t*>(buf.data());
    }
    catch(ara::error_not_found&)
    {
    }

#ifdef CHEAP_CLONING
    if(cfg.clone_from_a_frozen_volume_data_structure_)
    {
        bool emancipated = false;

        try
        {
            emancipated = cluster_.exists(emancipated_key_);
        }
        catch(ara::error_not_found&)
        {
            emancipated = false;
        }

        if(not emancipated)
        {
            LOG_INFO("Didn't find emancipated key for namespace " << ns_ <<
                     " -> checking whether volume is emancipated");
            ASSERT(not parent_page_dir_.empty());

            parent_cluster_.reset(new ara::
                                    Cluster(ara::
                                              ClusterID(cfg.clone_from_a_frozen_volume_data_structure_
                                                        ->frozen_parent_arakoon_cluster_name_)));
            parent_cluster_.add_nodes(cfg.clone_from_a_frozen_volume_data_structure_->frozen_parent_arakoon_nodes_);
            parent_cluster_->timeout(timeout_ms_);
            parent_cluster_->connect_master();
            {
                const std::string arakoon_parent_cork_uuid_(corkUUIDName(*parent_ns_));

                ara::buffer buf = parent_cluster_->get(arakoon_parent_cork_uuid_);
                std::string s(reinterpret_cast<char*>(buf.data()), buf.size());
                if(UUID(s) != cfg.clone_from_a_frozen_volume_data_structure_->parent_cork_uuid_)
                {
                    LOG_FATAL("Wrong cork uuid in parent metadatastore for clone");

                }
            }
            std::set<PageAddress> clone_addresses;
            {
                ara::value_list keys = cluster_.prefix(page_dir_);
                ara::value_list::iterator it = keys.begin();
                ara::arakoon_buffer buf;
                while(it.next(buf))
                {
                    PageAddress pa = get_pageaddress_from_key(buf,
                                                              page_dir_);
                    clone_addresses.insert(pa);
                }
            }

            ara::value_list keys = parent_cluster_->prefix(parent_page_dir_);
            LOG_INFO("got " << keys.size() << " parent key values.");
            ara::value_list::iterator it = keys.begin();
            ara::arakoon_buffer buf;

            while(it.next(buf))
            {
                PageAddress pa = get_pageaddress_from_key(buf,
                                                          parent_page_dir_);
                if(clone_addresses.count(pa))
                {
                    LOG_DEBUG("Page " << pa << " was already in the clone mdstore");

                }
                else
                {
                    LOG_DEBUG("Page " << pa << " is in the parent but not in the clone");
                    parent_keys.insert(pa);
                }
            }

            LOG_INFO("Parent still has " << parent_keys.size() << " relevant pages");
            if(parent_keys.empty())
            {
                LOG_INFO("Volume is emancipated");
                ara::sequence s;
                s.add_set(emancipated_key_);
                cluster_.synced_sequence(s);
                parent_cluster_.reset();
            }
        }
        else
        {
            LOG_INFO("Found emancipated key for namespace " << ns_ << " -> volume is emancipated");
        }
    }
#endif

    try
    {
        ara::buffer buf = cluster_.get(cork_key_);
        std::string s(reinterpret_cast<char*>(buf.data()), buf.size());
        LOG_INFO("cork uuid on restart was" << s);
    }
    catch(ara::error_not_found&)
    {
        LOG_INFO("No cork uuid on construction");
    }
    LOG_INFO(ns_ << ": metadata backend ready");
}

ArakoonMetaDataBackend::~ArakoonMetaDataBackend()
{
    try
    {
        if (delete_global_artefacts == DeleteGlobalArtefacts::T)
        {
            clear_all_keys();
        }
        else
        {
            sync();
        }
    }
    CATCH_STD_ALL_LOG_IGNORE(ns_ <<
                             ": failed to sync metadata pages to Arakoon");
}

void
ArakoonMetaDataBackend::sync()
{
    LOG_INFO("sync on " << ns_);
    try
    {
        flush_write_sequence_(true);
    }
    CATCH_STD_ALL_LOG_RETHROW(ns_ << ": syncing failed")
}

// void
// ArakoonMetaDataBackend::debugPrint()
// {
//     ara::value_list keys = cluster_.prefix(get_key("", ns_));
//     ara::value_list::iterator it = keys.begin();
//     ara::arakoon_buffer buf;
//     while(it.next(buf))
//     {
//         // if(buf.first == prefix_.size() + sizeof(PageAddress))
//         // {
//         //     PageAddress pa = pageaddressFromArakoonKey(buf,
//         //                                                prefix_);

//         //     std::cerr << "Found a key at " << std::hex << pa << std::endl;
//         // }
//         // else
//         // {
//         //     //       std::string key_string((char*)buf.second, buf.first);
//         //     ara::buffer b = cluster_.get(buf);
//         //     std::cerr << "key " <<  std::string((char*)buf.second, buf.first)
//         //               <<  ", value: " << std::string((char*)b.data(), b.size()) << std::endl;
//         // }
//     }
// }

bool
ArakoonMetaDataBackend::getPage(CachePage& p,
                                bool try_parent)
{
    LOG_TRACE(ns_ << ": page address " << p.page_address() << ", try_parent " <<
              try_parent);

    ASSERT(not try_parent or parent_cluster_);

    // Y42
    ArakoonMetaDataPageKey key(try_parent ? parent_page_dir_ : page_dir_,
                               p.page_address());

    try
    {
        ara::buffer b = (try_parent ? *parent_cluster_ : cluster_).get(key);
        std::pair<size_t, void*> buf = b.release();
        VERIFY(buf.first == CachePage::size());
        memcpy(p.data(), buf.second, buf.first);
        return true;
    }
    catch (ara::error_not_found&)
    {
        LOG_DEBUG(ns_ << ": couldn't get key from this arakoon");
        return false;
    }
    CATCH_STD_ALL_LOG_RETHROW(ns_ << ": couldn't get key from arakoon")
}

// bool
// ArakoonMetaDataBackend::pageExists(const PageAddress& pa)
// {
//     // Does this need to work with the parent stuff too?
//     LOG_TRACE("page address " << pa);

//     ArakoonMetaDataPageKey key(page_dir_,
//                                pa);
//     return cluster_.exists(key);

// }

bool
ArakoonMetaDataBackend::getPage(CachePage& p)
{
    LOG_TRACE(p.page_address());

    if (get_page_from_write_sequence_(p))
    {
        return true;
    }

    if(getPage(p,
               false))
    {
        return true;
    }
    else if(parent_cluster_ and
            getPage(p,
                    true))
    {
        p.incrementPageCloneID();
        ArakoonMetaDataPageKey key(page_dir_,
                                   p.page_address());

        try
        {
            cluster_.set(key, p);
        }
        CATCH_STD_ALL_LOG_RETHROW(ns_ << ": set failed")

        if(parent_keys.erase(p.page_address()))
        {
            LOG_DEBUG("Removing page " << p.page_address() << " from the parent keys");
            if(parent_keys.empty())
            {
                LOG_INFO("parent_keys for namespace " << ns_ << " is empty => volume becomes emancipated");
                ara::sequence s;
                s.add_set(emancipated_key_);

                try
                {
                    cluster_.synced_sequence(s);
                }
                CATCH_STD_ALL_LOG_RETHROW(ns_ << ": failed to set emancipated key")

                parent_cluster_.reset();
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool
ArakoonMetaDataBackend::pageExistsInParent(const PageAddress pa) const
{
    if (parent_ns_)
    {
        const ArakoonMetaDataPageKey key(parent_page_dir_,
                                         pa);
        ASSERT(parent_cluster_);
        try
        {
            return parent_cluster_->exists(key);
        }
        CATCH_STD_ALL_LOG_RETHROW(ns_ << ": failed to check existence of key")
    }
    else
    {
        return false;
    }
}

void
ArakoonMetaDataBackend::putPage(const CachePage& p,
                                const int32_t used_clusters_delta)
{
    LOG_TRACE("page address " << p.page_address() << ", used_clusters_delta " <<
              used_clusters_delta);

    int64_t used = used_clusters_ + used_clusters_delta;
    ASSERT(used >= 0);

    if (write_sequence_.size() == write_sequence_data_.size())
    {
        LOG_ERROR(ns_ << "write sequence is filled up - cannot put page");
        throw fungi::IOException("arakoon write sequence is filled up",
                                 ns_.str().c_str());
    }

    write_sequence_.emplace_back(p, &write_sequence_data_[write_sequence_.size()]);

    try
    {
        maybe_flush_write_sequence_();
        used_clusters_ = used;
    }
    CATCH_STD_ALL_LOG_RETHROW(ns_ << ": failed to put page")
}

void
ArakoonMetaDataBackend::discardPage(const CachePage& p,
                                    const int32_t used_clusters_delta)
{
    LOG_TRACE("page address " << p.page_address() << ", used_clusters_delta " <<
              used_clusters_delta);

    // We first need to flush the current write sequence:
    // The DISCARD could be issued for a page that doesn't exist in the backend, which
    // leads to a Not_found error for the page and to the whole sequence thus being
    // aborted.
    flush_write_sequence_(false);

    ASSERT(write_sequence_.empty());

    int64_t used = used_clusters_ + used_clusters_delta;
    ASSERT(used >= 0);

    ara::sequence s;
    ArakoonMetaDataPageKey key(page_dir_,
                               p.page_address());

    s.add_delete(key);
    s.add_set(used_clusters_key_, used);

    try
    {
        cluster_.sequence(s);
        used_clusters_ = used;
    }
    catch (ara::error_not_found&)
    {
        // swallow it
    }
    CATCH_STD_ALL_LOG_RETHROW(ns_ << ": failed to discard page")
}

void
ArakoonMetaDataBackend::clear_all_keys()
{
  LOG_INFO("reset on " << ns_ << ": deleting prefix");

  write_sequence_.clear();

  try
  {
      cluster_.delete_prefix(get_key("", ns_),
                             true);
  }
  CATCH_STD_ALL_LOG_RETHROW(ns_ << ": delete_prefix failed")
}


void
ArakoonMetaDataBackend::setCork(const yt::UUID& cork_uuid)
{
    LOG_TRACE("setting the cork_uuid_ to " << cork_uuid.str());

    auto seq(build_write_sequence_());
    seq.add_set(cork_key_,
                cork_uuid.str());

    try
    {
        flush_write_sequence_(seq, false);
    }
    CATCH_STD_ALL_LOG_RETHROW(ns_ << ": failed to set cork uuid " << cork_uuid.str())
}

boost::optional<yt::UUID>
ArakoonMetaDataBackend::lastCorkUUID()
{
    try
    {
        flush_write_sequence_(false);

        ara::buffer buf = cluster_.get(cork_key_);
        std::string s(reinterpret_cast<char*>(buf.data()), buf.size());
        return boost::optional<yt::UUID>(UUID(s));
    }
    catch(ara::error_not_found&)
    {
        return boost::none;
    }
    CATCH_STD_ALL_LOG_RETHROW(ns_ << ": failed to get last cork uuid")
}


yt::UUID
ArakoonMetaDataBackend::setFrozenParentCloneCork()
{
    ASSERT(parent_ns_);
    try
    {
        flush_write_sequence_(false);

        ara::buffer buf = cluster_.get(corkUUIDName(*parent_ns_));
        std::string s(reinterpret_cast<char*>(buf.data()), buf.size());

        LOG_INFO(ns_ << ": setting the cork_uuid_ to" << s);

        ara::sequence seq;
        seq.add_set(cork_key_,
                    s);
        cluster_.synced_sequence(seq);

        ASSERT(lastCorkUUID());

        return yt::UUID(s);
    }
    catch(ara::error_not_found&)
    {
        LOG_ERROR(ns_ << ": no cork uuid on a frozen parent??");
        throw;
    }
    CATCH_STD_ALL_LOG_RETHROW(ns_ << ": failed to set cork uuid")
}


MetaDataStoreFunctor&
ArakoonMetaDataBackend::for_each(MetaDataStoreFunctor& f,
                                 const ClusterAddress ca_max)
{
    flush_write_sequence_(false);

    const size_t page_size = CachePage::capacity();
    std::vector<ClusterLocationAndHash> clh(page_size);

    // this can be sped up by using arakoons range api
    for (ClusterAddress ca = 0; ca < ca_max; ca += page_size)
    {
        PageAddress pa = CachePage::pageAddress(ca);

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

ara::sequence
ArakoonMetaDataBackend::build_write_sequence_()
{
    LOG_TRACE(ns_);

    ara::sequence seq;
    for (const auto& e : write_sequence_)
    {
        LOG_TRACE(ns_ << ": adding page " << e.page_address() << " to sequence");
        const ArakoonMetaDataPageKey key(page_dir_,
                                         e.page_address());
        seq.add_set(key, e);
    }

    write_sequence_.clear();
    return seq;
}

void
ArakoonMetaDataBackend::flush_write_sequence_(ara::sequence& seq, bool sync)
{
    LOG_TRACE(ns_ << ", sync: " << sync);

    const int64_t used = used_clusters_;

    seq.add_set(used_clusters_key_, used);

    try
    {
        if (sync)
        {
            cluster_.synced_sequence(seq);
        }
        else
        {
            cluster_.sequence(seq);
        }
    }
    CATCH_STD_ALL_LOG_RETHROW(ns_ << ": failed to run sequence (sync: " << sync << ")");
}

void
ArakoonMetaDataBackend::flush_write_sequence_(bool sync)
{
    LOG_TRACE(ns_ << ", sync: " << sync);

    auto seq(build_write_sequence_());
    flush_write_sequence_(seq, sync);
}

void
ArakoonMetaDataBackend::maybe_flush_write_sequence_()
{
    LOG_TRACE(ns_);

    if (write_sequence_.size() == write_sequence_data_.size())
    {
        flush_write_sequence_(false);
    }
}

bool
ArakoonMetaDataBackend::get_page_from_write_sequence_(CachePage& page)
{
    LOG_TRACE(ns_ << ", address: " << page.page_address());

    for (auto rit = write_sequence_.rbegin(); rit != write_sequence_.rend(); ++rit)
    {
        const CachePage& p = *rit;
        if (page.page_address() == p.page_address())
        {
            page = p;
            return true;
        }
    }

    return false;
}

std::unique_ptr<MetaDataBackendConfig>
ArakoonMetaDataBackend::getConfig() const
{
    return std::unique_ptr<MetaDataBackendConfig>(new ArakoonMetaDataBackendConfig(config_));

}

MaybeScrubId
ArakoonMetaDataBackend::scrub_id()
{
    try
    {
        flush_write_sequence_(false);

        ara::buffer buf(cluster_.get(scrub_id_key_));
        const std::string s(reinterpret_cast<char*>(buf.data()),
                            buf.size());
        return ScrubId(yt::UUID(s));
    }
    catch (ara::error_not_found&)
    {
        return boost::none;
    }
    CATCH_STD_ALL_LOG_RETHROW(ns_ << ": failed to get scrub id");
}

void
ArakoonMetaDataBackend::set_scrub_id(const ScrubId& id)
{
    LOG_INFO(ns_ << ": setting the scrub ID to " << id);

    auto seq(build_write_sequence_());
    seq.add_set(scrub_id_key_,
                static_cast<const yt::UUID&>(id).str());

    try
    {
        flush_write_sequence_(seq, false);
    }
    CATCH_STD_ALL_LOG_RETHROW(ns_ << ": failed to set scrub ID " << id);
}

}

// Local Variables: **
// mode: c++ **
// End: **
