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

#ifndef SON_OF_KAK_H_
#define SON_OF_KAK_H_

#include "ClusterCacheDevice.h"
#include "ClusterCacheDeviceManagerT.h"
#include "ClusterCacheHandle.h"
#include "ClusterCacheKey.h"
#include "ClusterCacheMap.h"
#include "MountPointConfig.h"
#include "Types.h"
#include "VolumeConfig.h"
#include "VolumeDriverError.h"
#include "VolumeDriverParameters.h"
#include "ClusterCacheMode.h"
#include "ClusterLocationAndHash.h"

#include <atomic>
#include <algorithm>
#include <list>
#include <string>
#include <utility>
#include <vector>
#include <memory>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/Catchers.h>
#include <youtils/FileUtils.h>
#include <youtils/Logging.h>
#include <youtils/RWLock.h>
#include <youtils/Serialization.h>
#include <youtils/VolumeDriverComponent.h>
#include <youtils/Weed.h>

namespace volumedrivertest
{
class ClusterCacheTest;
}

namespace volumedriver
{

MAKE_EXCEPTION(MountPointNotConfigured, fungi::IOException);
MAKE_EXCEPTION(InvalidClusterCacheConfig, fungi::IOException);
MAKE_EXCEPTION(InvalidClusterCacheHandle, fungi::IOException);
MAKE_EXCEPTION(InvalidClusterCacheOperation, fungi::IOException);

struct SearchOldInNews
{
    SearchOldInNews(const MountPointConfig& mpc)
        : mpc_(mpc)
    {}

    bool
    operator()(const MountPointConfigs& mpcs,
               std::string& message)
    {
        for (const auto& mpc : mpcs)
        {
            if (mpc.path == mpc_.path)
            {
                if (mpc.size == mpc_.size)
                {
                    return true;
                }
                else
                {
                    std::stringstream ss;

                    ss << "Cannot resize mountpoint " << mpc_.path
                       << " from " << mpc_.size
                       << " to " << mpc.size;
                    message = ss.str();

                    return false;
                }
            }
        }
        std::stringstream ss;
        ss <<  "Cannot delete mountpoint " << mpc_.path;
        message = ss.str();

        return false;
    }
    const MountPointConfig& mpc_;
};

struct SearchNewInOlds
{
    SearchNewInOlds(const MountPointConfig& mpc)
        : mpc_(mpc)
    {}

    bool
    operator()(const MountPointConfigs& mpcs,
               std::string& message)
    {
        for (const auto& mpc : mpcs)
        {
            if (mpc.path == mpc_.path)
            {
                return true;
            }
        }

        std::stringstream ss;
        ss << "Cannot add mountpoint " << mpc_.path;
        message = ss.str();

        return false;
    }
    const MountPointConfig& mpc_;
};

struct MountPointChecker
{
    MountPointChecker(const MountPointConfigs& old_configs,
                      const MountPointConfigs& new_configs)
        : old_configs_(old_configs)
        , new_configs_(new_configs)
    {}

    bool
    operator()(ConfigurationReport& c_rep)
    {
        bool return_val = true;

        for (const auto& mp : old_configs_)
        {
            std::string message;

            SearchOldInNews searcher(mp);
            if (not searcher(new_configs_,
                             message))
            {
                return_val = false;

                ConfigurationProblem
                    problem(initialized_params::PARAMETER_TYPE(clustercache_mount_points)::name(),
                            initialized_params::PARAMETER_TYPE(clustercache_mount_points)::section_name(),
                            message);
                c_rep.push_front(problem);
            }
        }

        for (const auto& mp : new_configs_)
        {
            std::string message;

            SearchNewInOlds searcher(mp);
            if (not searcher(old_configs_,
                             message))
            {
                return_val = false;

                ConfigurationProblem
                    problem(initialized_params::PARAMETER_TYPE(clustercache_mount_points)::name(),
                            initialized_params::PARAMETER_TYPE(clustercache_mount_points)::section_name(),
                            message);
                c_rep.push_front(problem);
            }
        }
        return return_val;
    }

private:
    const MountPointConfigs& old_configs_;
    const MountPointConfigs& new_configs_;
};

/* cnanakos: Move them somewhere else? */
template<typename T>
struct DListNodeTraits
{
     typedef T node;
     typedef T* node_ptr;
     typedef const T* const_node_ptr;
     static node *get_next(const node* n) {return n->dnext();}
     static void set_next(node* n, node* next) {return n->dnext(next);}
     static node* get_previous(const node* n) {return n->dprevious();}
     static void set_previous(node* n, node* prev) {return n->dprevious(prev);}
};

template<typename T>
struct SListNodeTraits
{
     typedef T node;
     typedef T* node_ptr;
     typedef const T* const_node_ptr;
     static node_ptr get_next(const_node_ptr n) {return n->snext();}
     static void set_next(node_ptr n, node_ptr next) {return n->snext(next);}
};

template<typename T, template<class> class Traits>
struct NodeValueTraits
{
    typedef Traits<T> node_traits;
    typedef typename node_traits::node_ptr node_ptr;
    typedef typename node_traits::const_node_ptr const_node_ptr;
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    static node_ptr to_node_ptr(value_type& value) {return node_ptr(&value);}
    static const_node_ptr to_node_ptr(const value_type& value) {return const_node_ptr(&value);}
    static pointer to_value_ptr(node_ptr n) {return pointer(n);}
    static const_pointer to_value_ptr(const_node_ptr n) {return const_pointer(n);}
};

template<typename T>
struct DListNodeValueTraits: NodeValueTraits<T, DListNodeTraits>
{
    static const boost::intrusive::link_mode_type link_mode = boost::intrusive::auto_unlink;
};

template<typename T>
struct SListNodeValueTraits: NodeValueTraits<T, SListNodeTraits>
{
    static const boost::intrusive::link_mode_type link_mode = boost::intrusive::normal_link;
};

template<typename T,
         uint64_t logging_interval = (1 << 19)>
class ClusterCacheT
    : public VolumeDriverComponent
{
public:
    typedef T DeviceType;
    typedef ClusterCacheDeviceManagerT<T> ManagerType;

    ClusterCacheT& operator=(const ClusterCacheT&) = delete;
    ClusterCacheT(const ClusterCacheT&) = delete;

    typedef boost::archive::binary_iarchive iarchive_type;
    typedef boost::archive::binary_oarchive oarchive_type;

private:
    DECLARE_LOGGER("ClusterClusterCache");

    typedef DListNodeValueTraits<ClusterCacheEntry> DListClusterCacheEntryValueTraits;

    typedef SListNodeValueTraits<ClusterCacheEntry> SListClusterCacheEntryValueTraits;

    typedef boost::intrusive::value_traits<DListClusterCacheEntryValueTraits>
        DListClusterCacheEntryValueTraitsOption;

    typedef boost::intrusive::value_traits<SListClusterCacheEntryValueTraits>
        SListClusterCacheEntryValueTraitsOption;

    typedef boost::intrusive::list<ClusterCacheEntry,
                                   DListClusterCacheEntryValueTraitsOption,
                                   boost::intrusive::constant_time_size<false>> dlist_t;

    typedef ClusterCacheMap<ClusterCacheEntry,
                            SListClusterCacheEntryValueTraitsOption> cachemap_t;

    typedef boost::intrusive::circular_list_algorithms<DListNodeTraits<ClusterCacheEntry>> dlist_algo;

    // A ClusterCache Namespace (CNS, associated with a ClusterCacheHandle) is used to
    // limit the size of the cachemap it contains. The idea is to maintain a global
    // LRU and an LRU per CNS.
    // Allocation of an entry follows this scheme:
    // * no size limit / size limit not reached yet:
    // ** check invalid list / check free entries / check global LRU
    // * size limit and size limit reached:
    // * check the CNS'es LRU
    // .
    // NB: Yes, there's some potential for confusion with backend::Namespace - feel
    // free to rename to something better.
    struct Namespace
    {
        cachemap_t map;
        dlist_t lru;
        boost::optional<uint64_t> max_entries;

        Namespace() = default;

        ~Namespace() = default;

        Namespace(const Namespace&) = delete;

        Namespace&
        operator=(const Namespace&) = delete;

        BOOST_SERIALIZATION_SPLIT_MEMBER();

        template<typename Archive>
        void
        load(Archive& ar,
             const unsigned int version)
        {
            VERIFY(map.empty());
            VERIFY(lru.empty());

            if (version != 0)
            {
                THROW_SERIALIZATION_ERROR(version,
                                          0,
                                          0);
            }

            ar & max_entries;
            uint64_t size_exp;
            ar & size_exp;
            map.resize(size_exp);
        }

        template<typename Archive>
        void
        save(Archive& ar,
             const unsigned int /* version */) const
        {

            ar & max_entries;
            const uint64_t size_exp = map.spine_size_exp();
            ar & size_exp;
        }
    };

public:
    struct NamespaceInfo
    {
        NamespaceInfo(const ClusterCacheHandle h,
                      const Namespace& n)
            : handle(h)
            , entries(n.map.entries())
            , max_entries(n.max_entries)
            , map_stats(n.map.stats().size())
        {
            for (const auto& s : n.map.stats())
            {
                map_stats[s.first] = s.second;
            }
        }

        ClusterCacheHandle handle;
        uint64_t entries;
        boost::optional<uint64_t> max_entries;
        std::vector<uint64_t> map_stats;
    };

private:
    friend class volumedrivertest::ClusterCacheTest;
    friend class boost::serialization::access;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        clear_();

        if (version > 2)
        {
            THROW_SERIALIZATION_ERROR(version, 2, 2);
        }

        ar & manager_;

        typename ManagerType::ClusterCacheDeviceInMapper in_mapper(manager_);
        ar & in_mapper;
        uint32_t size;
        ar & size;

        if (version < 2)
        {
            int64_t size_exp;
            ar & size_exp;
            VERIFY(size_exp < 64);

            Namespace* nspace = maybe_create_namespace_(content_based_handle);
            nspace->map.resize(size_exp);
        }
        else
        {
            ar & namespaces_;
        }

        auto load_entry([&](T*& device,
                            ClusterCacheEntry*& entry)
                        {
                            // AR: reconsider (de)serialization before releasing!
                            // The following order would be preferrable:
                            //
                            //   index / offset / key / mode
                            //
                            // as it permits the following cleanup:
                            //
                            // ar & index;
                            // ar & offset;
                            // T* device = in_mapper.lookup(index);
                            // ClusterCacheEntry* e = device->getEntry(offset);
                            // ar & *e;
                            //
                            // version 0 is out there with the order index / key / offset
                            // version 1 has the order index / key / mode / offset, but
                            // it's not officially released yet so can easily be banned.
                            uint32_t index;
                            ar & index;

                            device = in_mapper.lookup(index);

                            ClusterCacheKey key(ClusterCacheHandle(0),
                                                0);
                            ar & key;

                            ClusterCacheMode mode = ClusterCacheMode::ContentBased;

                            if (version >= 1)
                            {
                                ar & mode;
                            }

                            uint32_t offset;
                            ar & offset;

                            if (device)
                            {
                                entry = device->getEntry(offset);
                                if (entry)
                                {
                                    entry = new(entry) ClusterCacheEntry(key,
                                                                         mode);
                                }
                            }
                        });


        for (uint32_t i = 0; i < size; ++i)
        {
            if ((i % logging_interval) == 0)
            {
                LOG_INFO("Deserializing metadata of entries, still " << (size - i) <<
                         " to go");
            }

            T* device = nullptr;
            ClusterCacheEntry* entry = nullptr;

            load_entry(device,
                       entry);

            if (entry)
            {
                VERIFY(device);

                if ((i % test_frequency_) == 0)
                {
                    device->check(*entry);
                }

                Namespace* nspace = find_namespace_(make_handle_(entry->mode(),
                                                                 entry->key));

                VERIFY(nspace);
                nspace->map.insert(*entry);
                if (nspace->max_entries)
                {
                    VERIFY(nspace->map.entries() <= *nspace->max_entries);
                    nspace->lru.push_back(*entry);
                }
                else
                {
                    lru_.push_back(*entry);
                }
            }
        }

        if (version >= 1)
        {
            size = 0;
            ar & size;
            for (uint32_t i = 0; i < size; ++i)
            {
                if ((i % logging_interval) == 0)
                {
                    LOG_INFO("Deserializing metadata of invalidated entries, still " <<
                             (size - i) << " to go");
                }

                T* device = nullptr;
                ClusterCacheEntry* entry = nullptr;

                load_entry(device,
                           entry);
                if (entry)
                {
                    invalidated_entries_.push_back(*entry);
                }
            }
        }
    }

    template<class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        if (version != 2)
        {
            THROW_SERIALIZATION_ERROR(version, 2, 2);
        }

        ar & manager_;

        typename ManagerType::ClusterCacheDeviceOutMapper out_mapper(manager_);
        ar & out_mapper;

        uint32_t size = 0;
        for (const auto& v : namespaces_)
        {
            size += v.second->map.entries();
        }

        ar & size;
        ar & namespaces_;

        auto save_entry([&](const ClusterCacheEntry& entry)
                        {
                            const T* device = manager_.getDeviceFromEntry(&entry);
                            ASSERT(device);
                            uint32_t index = out_mapper.lookup(device);
                            uint32_t offset = device->getIndex(&entry);

                            // cf. comment on ordering in load()!
                            ar & index;
                            ar & entry.key;
                            ClusterCacheMode mode = entry.mode();
                            ar & mode;
                            ar & offset;
                        });

        uint32_t k = 0;

        auto save_entries([&](const char* what,
                              const dlist_t& list)
                          {
                              for (const ClusterCacheEntry& entry : list)
                              {
                                  if ((k % logging_interval) == 0)
                                  {
                                      LOG_INFO("Serializing metadata of " << what <<
                                               ", still " << (size - k) << " to go" );
                                  }

                                  k++;
                                  save_entry(entry);
                              }
                          });

        for (const auto& v : namespaces_)
        {
            save_entries("entries",
                         v.second->lru);
        }

        save_entries("entries",
                     lru_);

        size = invalidated_entries_.size();
        ar & size;
        k = 0;

        save_entries("invalidated entries",
                     invalidated_entries_);
    }

    typedef boost::mutex register_lock_type;
    register_lock_type register_lock_;

    mutable fungi::RWLock rwlock;
    mutable boost::mutex listlock;

    ManagerType manager_;

private:
    using NamespaceMap = std::map<ClusterCacheHandle, std::unique_ptr<Namespace>>;
    NamespaceMap namespaces_;

    dlist_t invalidated_entries_;
    dlist_t lru_;
    std::atomic<uint64_t> num_hits;
    std::atomic<uint64_t> num_misses;

private:
    Namespace*
    find_namespace_(const ClusterCacheHandle handle) const
    {
        auto it = namespaces_.find(handle);
        if (it != namespaces_.end())
        {
            return it->second.get();
        }
        else
        {
            return nullptr;
        }
    }

    Namespace*
    find_namespace_or_throw_(const ClusterCacheHandle handle) const
    {
        Namespace* nspace = find_namespace_(handle);
        if (nspace == nullptr)
        {
            LOG_ERROR(handle << ": no such ClusterCacheNamespace");
            throw InvalidClusterCacheHandle("no such ClusterCacheNamespace");
        }

        return nspace;
    }

    Namespace*
    maybe_create_namespace_(const ClusterCacheHandle handle)
    {
        Namespace* nspace = find_namespace_(handle);
        if (not nspace)
        {
            auto ns(std::make_unique<Namespace>());
            ns->map.resize(average_entries_per_bin.value(),
                           manager_.totalSizeInEntries());

            auto res(namespaces_.emplace(handle,
                                         std::move(ns)));
            VERIFY(res.second);
            nspace = res.first->second.get();
        }

        return nspace;
    }

    static ClusterCacheHandle
    make_handle_(const ClusterCacheMode mode,
                 const ClusterCacheKey& key)
    {
        switch (mode)
        {
        case ClusterCacheMode::ContentBased:
            return content_based_handle;
        case ClusterCacheMode::LocationBased:
            return key.cluster_cache_handle();
        }

        UNREACHABLE;
        VERIFY(0 == "venturing into unchartered code paths");
    }

    ClusterCacheEntry*
    get_invalidated_cache_entry_()
    {
        if (not invalidated_entries_.empty())
        {
            ClusterCacheEntry* entry = &invalidated_entries_.back();
            invalidated_entries_.pop_back();
            return entry;
        }
        return nullptr;
    }

    ClusterCacheMode
    get_cache_entry_mode(const ClusterCacheHandle handle)
    {
        return (handle == content_based_handle ? ClusterCacheMode::ContentBased :
                ClusterCacheMode::LocationBased);
    }

    void
    unlink_entry_from_dlist_(ClusterCacheEntry& entry)
    {
        dlist_algo::unlink(&entry);
        dlist_algo::init(&entry);
    }

public:
    ClusterCacheT(const boost::property_tree::ptree& pt)
        : VolumeDriverComponent(RegisterComponent::T,
                                pt)
        , register_lock_()
        , rwlock("rwlock")
        , num_hits(0)
        , num_misses(0)
        , serialize_read_cache(pt)
        , read_cache_serialization_path(pt)
        , average_entries_per_bin(pt)
        , clustercache_mount_points(pt)
    {
        fs::path serialization_path(getClusterCacheSerializationPath());
        if (serialize_read_cache.value())
        {
            if (fs::exists(serialization_path))
            {
                try
                {
                    fs::ifstream ifs(serialization_path);
                    iarchive_type ia(ifs);
                    ia & *this;
                }
                CATCH_STD_ALL_EWHAT({
                        LOG_ERROR("Problem reinstating the cache: " << EWHAT <<
                                  " - starting from clean slate");
                        clear_();
                    });

                fs::remove(serialization_path);
            }
            else
            {
                LOG_WARN("No file found for cache deserialization");
            }
        }
        else
        {
            if (fs::exists(serialization_path))
            {
                LOG_INFO("Removing cache serialization from a previous run");
                fs::remove(serialization_path);
            }
        }

        unsigned added = 0, not_added = 0;

        for (auto it = clustercache_mount_points.value().begin();
             it != clustercache_mount_points.value().end();
             ++it)
        {
            if (maybeAddDevice(it->path,
                               it->size))
            {
                ++added;
            }
            else
            {
                ++not_added;
            }
        }

        LOG_INFO("Added " << added << " devices, "
                 << " reinstated, " << not_added << " skipped");

        Namespace* cns = maybe_create_namespace_(content_based_handle);
        VERIFY(cns);
    }

    virtual void
    update(const boost::property_tree::ptree& pt,
           UpdateReport& u_rep) override final
    {
        serialize_read_cache.update(pt,u_rep);
        read_cache_serialization_path.update(pt,u_rep);
        average_entries_per_bin.update(pt,u_rep);
        initialized_params::PARAMETER_TYPE(clustercache_mount_points) new_mount_points(pt);

        for (const auto& mp : new_mount_points.value())
        {
            maybeAddDevice(mp.path,
                           mp.size);
        }
    }

    virtual void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault reportDefault) const override final
    {
        serialize_read_cache.persist(pt,
                                     reportDefault);
        read_cache_serialization_path.persist(pt,
                                              reportDefault);
        average_entries_per_bin.persist(pt,
                                        reportDefault);
        clustercache_mount_points.persist(pt,
                                          reportDefault);
    }

    virtual const char*
    componentName() const override final
    {
        return "Kontent Addressed Cache";
    }

    virtual bool
    checkConfig(const boost::property_tree::ptree& pt,
                ConfigurationReport& c_rep) const override final
    {
        initialized_params::PARAMETER_TYPE(clustercache_mount_points) new_mount_points(pt);
        MountPointChecker mpc(clustercache_mount_points.value(),
                              new_mount_points.value());
        return  mpc(c_rep);
    }

    ~ClusterCacheT()
    {
        if (serialize_read_cache.value())
        {
            try
            {
                manager_.sync();
                fs::path serialization_path(getClusterCacheSerializationPath());
                FileUtils::removeFileNoThrow(serialization_path);
                fs::ofstream ofs(serialization_path);
                oarchive_type oa(ofs);
                oa & *this;
            }
            CATCH_STD_ALL_LOG_IGNORE("Could not serialize the cache state");
        }
    }

    // Mapping:
    // content_based_handle -> ContentBased
    // _ -> LocationBased
    //
    // the mapping to OwnerTag relies on the assertion that OwnerTag(0) must not be
    // used (cf. OwnerTag.h).
    ClusterCacheHandle
    registerVolume(const OwnerTag otag,
                   const ClusterCacheMode mode)
    {
        VERIFY(otag != OwnerTag(0)); // cf. OwnerTag.h

        const ClusterCacheHandle handle(mode == ClusterCacheMode::LocationBased ?
                                        static_cast<uint64_t>(otag) :
                                        0);

        fungi::ScopedWriteLock l(rwlock);

        if (mode == ClusterCacheMode::ContentBased)
        {
            deregister_(ClusterCacheHandle(static_cast<uint64_t>(otag)));
        }

        maybe_create_namespace_(handle);
        return handle;
    }

    void
    deregisterVolume(const OwnerTag otag)
    {
        // cf. OwnerTag.h
        VERIFY(otag != OwnerTag(0));
        const ClusterCacheHandle handle(static_cast<uint64_t>(otag));

        fungi::ScopedWriteLock l(rwlock);
        deregister_(handle);
    }

    void
    set_max_entries(const ClusterCacheHandle handle,
                    const boost::optional<uint64_t> limit)
    {
        if (limit and *limit == 0)
        {
            LOG_ERROR(handle << ": max entries must be > " << limit);
            throw InvalidClusterCacheConfig("Invalid max entries");
        }

        fungi::ScopedWriteLock l(rwlock);

        Namespace* nspace = find_namespace_or_throw_(handle);

        LOG_INFO(handle << ": changing max entries from " <<
                 nspace->max_entries << " to " << limit);

        if (nspace->max_entries)
        {
            if (limit)
            {
                const int64_t surplus = nspace->map.entries() - *limit;
                for (int64_t i = 0; i < surplus; ++i)
                {
                    VERIFY(not nspace->lru.empty());
                    ClusterCacheEntry& e = nspace->lru.back();
                    nspace->lru.pop_back();
                    nspace->map.remove(e);
                    invalidated_entries_.push_front(e);
                }

                VERIFY(nspace->map.entries() <= *limit);
            }
            else
            {
                while (not nspace->lru.empty())
                {
                    ClusterCacheEntry& e = nspace->lru.front();
                    nspace->lru.pop_front();
                    lru_.push_back(e);
                }
            }
        }
        else
        {
            VERIFY(nspace->lru.empty());
            if (limit)
            {
                int64_t surplus = nspace->map.entries() - *limit;

                if (surplus > 0)
                {
                    LOG_INFO(handle << ": imposing a max entries limit of " <<
                             *limit <<
                             " on a previously unlimited namespace that contains " <<
                             nspace->map.entries() <<
                             " - this is expensive and loses LRU information!");
                }

                dlist_t to_invalidate;
                nspace->map.for_each([&](ClusterCacheEntry& e)
                                     {
                                         unlink_entry_from_dlist_(e);
                                         if (surplus > 0)
                                         {
                                             to_invalidate.push_front(e);
                                             --surplus;
                                         }
                                         else
                                         {
                                             nspace->lru.push_front(e);
                                         }
                                     });

                while (not to_invalidate.empty())
                {
                    ClusterCacheEntry& e = to_invalidate.front();
                    to_invalidate.pop_front();
                    const bool ok = nspace->map.remove(e);
                    VERIFY(ok);
                    invalidated_entries_.push_front(e);
                }

                VERIFY(nspace->map.entries() <= *limit);
            }
        }

        nspace->map.resize(average_entries_per_bin.value(),
                           limit ?
                           *limit :
                           manager_.totalSizeInEntries());

        nspace->max_entries = limit;
    }

    boost::optional<uint64_t>
    get_max_entries(const ClusterCacheHandle handle) const
    {
        fungi::ScopedReadLock l(rwlock);

        Namespace* nspace = find_namespace_or_throw_(handle);
        return nspace->max_entries;
    }

    NamespaceInfo
    namespace_info(const ClusterCacheHandle handle) const
    {
        fungi::ScopedReadLock l(rwlock);

        Namespace* nspace = find_namespace_or_throw_(handle);
        return NamespaceInfo(handle,
                             *nspace);
    }

    void
    remove_namespace(const ClusterCacheHandle handle)
    {
        if (handle == content_based_handle)
        {
            LOG_ERROR("Cannot remove the handle for ContentBased entries");
            throw InvalidClusterCacheOperation("Cannot remove the handle for ContentBased entries");
        }

        fungi::ScopedWriteLock l(rwlock);
        deregister_(handle);
    }

    std::vector<ClusterCacheHandle>
    list_namespaces() const
    {
        std::vector<ClusterCacheHandle> vec;
        fungi::ScopedReadLock l(rwlock);
        vec.reserve(namespaces_.size());

        for (const auto& v : namespaces_)
        {
            vec.push_back(v.first);
        }

        return vec;
    }

    void
    invalidate(const ClusterCacheHandle handle,
               const ClusterAddress& ca,
               const youtils::Weed& weed)
    {
        invalidate(handle,
                   handle != content_based_handle ?
                   ClusterCacheKey(handle,
                                   ca) :
                   ClusterCacheKey(weed));
    }

    void
    invalidate(const ClusterCacheHandle handle,
               const ClusterAddress ca)
    {
        VERIFY(handle != content_based_handle);

        invalidate(handle,
                   ClusterCacheKey(handle,
                                   ca));
    }

    void
    invalidate(const ClusterCacheHandle handle,
                const ClusterCacheKey& key)
    {
        if (handle != content_based_handle)
        {
            fungi::ScopedWriteLock l(rwlock);
            Namespace* nspace = find_namespace_(handle);
            VERIFY(nspace);
            ClusterCacheEntry* entry = nspace->map.find(key);
            if (entry)
            {
                nspace->map.remove(*entry);
                unlink_entry_from_dlist_(*entry);
                invalidated_entries_.push_back(*entry);
            }
        }
    }

    void
    add(const ClusterCacheHandle handle,
        const ClusterAddress ca,
        const youtils::Weed& weed,
        const uint8_t* buf)
    {
        if (handle != content_based_handle)
        {
            add(handle,
                ClusterCacheKey(handle,
                                ca),
                buf);
        }
        else if (weed != youtils::Weed::null())
        {
            add(handle,
                ClusterCacheKey(weed),
                buf);
        }
    }

    void
    add(const ClusterCacheHandle handle,
        const ClusterCacheKey& key,
        const uint8_t* buf)
    {
        fungi::ScopedWriteLock l(rwlock);

        // Really only serves as documentation - the rwlock should rule
        // out concurrent accesses already hence we can be lazy and use
        // listlock rather coarsely.
        boost::lock_guard<decltype(listlock)> llg(listlock);

        Namespace* nspace = find_namespace_(handle);
        VERIFY(nspace);

        bool reinit = true;
        T* read_cache = nullptr;

        ClusterCacheEntry* entry = nspace->map.find(key);
        if (entry)
        {
            /* ContentBased cache is immutable */
            if (handle == content_based_handle)
            {
                return;
            }
            /* This means that the entry has not been invalidated yet
             * but needs a buffer update. LocationBased cache is
             * mutable.
             */
            reinit = false;
            unlink_entry_from_dlist_(*entry);
        }

        if (not entry and
            nspace->max_entries and
            nspace->map.entries() == *nspace->max_entries)
        {
            // the namespace reached its size limit - recycle an entry from its
            // private LRU
            if (*nspace->max_entries == 0)
            {
                LOG_DEBUG("namespace " << handle << " is misconfigured with size 0, not caching anything");
                return;
            }

            dlist_t& lru = nspace->lru;
            VERIFY(not lru.empty());
            entry = &lru.back();
            lru.pop_back();
            const bool ignore = nspace->map.remove(*entry);
            VERIFY(ignore);
        }

        if (not entry)
        {
            /* Try to allocate an invalidated entry first */
            entry = get_invalidated_cache_entry_();
        }

        if (not entry)
        {
            /* otherwise get the next free one */
            entry = manager_.getNextFreeCluster(key,
                                                read_cache);
        }

        if (not entry and not lru_.empty())
        {
            // finally we have no other option but to recycle an existing one
            // from the global LRU list
            entry = &lru_.back();
            lru_.pop_back();

            Namespace* old_nspace = find_namespace_(make_handle_(entry->mode(),
                                                                 entry->key));
            VERIFY(old_nspace);
            const bool ignore = old_nspace->map.remove(*entry);
            VERIFY(ignore);
        }

        if (not entry)
        {
            LOG_WARN("Failed to allocate an entry for handle " << handle <<
                     " - are all devices gone or all entries consumed by other namespaces?");
            return;
        }

        VERIFY(entry);

        if (not read_cache)
        {
            read_cache = manager_.getDeviceFromEntry(entry);
        }

        VERIFY(read_cache);

        if (reinit)
        {
            entry = new(entry) ClusterCacheEntry(key,
                                                 get_cache_entry_mode(handle));
            nspace->map.insert(*entry);
        }

        if (nspace->max_entries)
        {
            VERIFY(nspace->map.entries() <= *nspace->max_entries);
            nspace->lru.push_front(*entry);
        }
        else
        {
            lru_.push_front(*entry);
        }

        ssize_t res = read_cache->write(buf,
                                        entry);
        if (res != (ssize_t)T::cluster_size_)
        {
            LOG_ERROR("Couldn't write to " << read_cache << " - offlining it");
            offlineDevice(read_cache);
        }
    }

    bool
    read(const ClusterCacheHandle handle,
         const ClusterAddress ca,
         const youtils::Weed& weed,
         uint8_t* buf)
    {
        if (handle != content_based_handle)
        {
            return read(handle,
                        ClusterCacheKey(handle,
                                        ca),
                        buf);
        }
        else if (weed != youtils::Weed::null())
        {
            return read(handle,
                        ClusterCacheKey(weed),
                        buf);
        }
        else
        {
            ++num_misses;
            return false;
        }
    }

    bool
    read(const ClusterCacheHandle handle,
         const ClusterCacheKey& key,
         uint8_t* buf)
    {
        T* read_cache = 0;
        {
            fungi::ScopedReadLock l(rwlock);
            Namespace* nspace = find_namespace_(handle);
            VERIFY(nspace);
            ClusterCacheEntry* entry = nspace->map.find(key);
            if (not entry)
            {
                ++num_misses;
                return false;
            }
            else
            {
                // VERIFY(entry->read_cache_);
                // quicker... just ask the manager to read...
                read_cache = manager_.getDeviceFromEntry(entry);
                ASSERT(read_cache);
                ssize_t res = read_cache->read(buf,
                                               entry);

                if ((ssize_t)T::cluster_size_ == res)
                {
                    num_hits++;

                    dlist_t& lru = nspace->max_entries ?
                        nspace->lru :
                        lru_;

                    boost::lock_guard<decltype(listlock)> llg(listlock);
                    unlink_entry_from_dlist_(*entry);
                    lru.push_front(*entry);
                    return true;
                }
                else
                {
                    LOG_ERROR("Couldn't read from " << read_cache << " - offlining it");
                }
            }
        }

        fungi::ScopedWriteLock l(rwlock);
        offlineDevice(read_cache);
        ++num_misses;
        return false;
    }

    void
    get_stats(uint64_t& hits,
              uint64_t& misses,
              uint64_t& entries)
    {
        fungi::ScopedReadLock ls(rwlock);

        hits = num_hits;
        misses = num_misses;
        entries = 0;

        for (const auto& v : namespaces_)
        {
            entries += v.second->map.entries();
        }
    }

    void
    remove_device_from_list_and_delete(dlist_t& list,
                                       T* read_cache)
    {
        auto it = list.begin();
        while (it != list.end())
        {
            ClusterCacheEntry& e = *it;
            if (manager_.getDeviceFromEntry(&e) == read_cache)
            {
                it = list.erase(it);

                Namespace* nspace = find_namespace_(make_handle_(e.mode(),
                                                                 e.key));
                VERIFY(nspace);
                bool ignore = nspace->map.remove(e);
                VERIFY(ignore);
            }
            else
            {
                ++it;
            }
        }
    }

    void
    offlineDevice(T* read_cache,
                  const bool log_error = true)
    {
        rwlock.assertWriteLocked();

        VERIFY(read_cache);

        if (not manager_.isStillClusterCacheDevice(read_cache))
        {
            LOG_INFO("Device already offlined");
            return;
        }

        if (log_error)
        {
            std::stringstream ss;
            ss << read_cache->info().path << " offlined";
            VolumeDriverError::report(events::VolumeDriverErrorCode::ClusterCacheMountPointOfflined,
                                      ss.str());
        }

        LOG_INFO("Offlining read_cache " << read_cache);

        remove_device_from_list_and_delete(lru_,
                                           read_cache);
        remove_device_from_list_and_delete(invalidated_entries_,
                                           read_cache);
        for (auto& v : namespaces_)
        {
            remove_device_from_list_and_delete(v.second->lru,
                                               read_cache);
        }

        manager_.removeDevice(read_cache);
    }

    void
    offlineDevice(const fs::path& path)
    {
        //throw if it's not configured
        mountpointconfig_from_path(path);

        T* read_cache = manager_.getDeviceFromPath(path);
        if (read_cache)
        {
            fungi::ScopedWriteLock l(rwlock);
            offlineDevice(read_cache);
        }
    }

    void
    onlineDevice(const fs::path& path)
    {
        MountPointConfig mp_cfg = mountpointconfig_from_path(path);
        maybeAddDevice(mp_cfg.path, mp_cfg.size);
    }

    void
    deviceInfo(typename ManagerType::Info& info)
    {
        manager_.info(info);
    }

    uint64_t
    totalSizeInEntries()
    {
        return manager_.totalSizeInEntries();
    }

    void
    fuckup_fd_forread()
    {
        manager_.fuckup_fd_forread();
    }

    void
    fuckup_fd_forwrite()
    {
        manager_.fuckup_fd_forwrite();
    }

private:
    DECLARE_PARAMETER(serialize_read_cache);
    DECLARE_PARAMETER(read_cache_serialization_path);
    DECLARE_PARAMETER(average_entries_per_bin);
    DECLARE_PARAMETER(clustercache_mount_points);

    static constexpr uint64_t test_frequency_ = 8192;
    static const ClusterCacheHandle content_based_handle;

    bool
    maybeAddDevice(const fs::path& path,
                   const uint64_t size)
    {
        if (manager_.getDeviceFromPath(path))
        {
            LOG_INFO("Not adding " << path
                     << " because it is already present");
            return false;
        }
        else
        {
            LOG_INFO("Adding " << path);
            return manager_.addDevice(path,
                                      size);
        }
    }


    fs::path
    getClusterCacheSerializationPath()
    {
        return fs::path(read_cache_serialization_path.value()) / ".read_cache_serialization";
    }

    MountPointConfig
    mountpointconfig_from_path(const fs::path& path) const
    {
        for (const MountPointConfig& mpc : clustercache_mount_points.value())
        {
            if (mpc.path == path)
            {
                return mpc;
            }
        }
        {
            std::stringstream ss;
            ss << "Unconfigured device path " << path;
            LOG_ERROR(ss.str());
            throw MountPointNotConfigured(ss.str().c_str());
        }
    }

    void
    deregister_(const ClusterCacheHandle handle)
    {
        auto it = namespaces_.find(handle);
        if (it != namespaces_.end())
        {
            it->second->map.for_each([&](ClusterCacheEntry& e)
                                     {
                                         unlink_entry_from_dlist_(e);
                                         invalidated_entries_.push_front(e);
                                     });
            namespaces_.erase(it);
        }
    }

    void
    clear_()
    {
        manager_.clear();
        namespaces_.clear();
        lru_.clear();
        invalidated_entries_.clear();
    }
};

template<typename T,
         uint64_t N>
const ClusterCacheHandle ClusterCacheT<T, N>::content_based_handle = ClusterCacheHandle(0);

typedef ClusterCacheT<ClusterCacheDevice> ClusterCache;

}

BOOST_CLASS_VERSION(volumedriver::ClusterCache, 2);
BOOST_CLASS_VERSION(volumedriver::ClusterCache::Namespace, 0);

#endif // SON_OF_KAK_H_

// Local Variables: **
// mode: c++ **
// End: **
