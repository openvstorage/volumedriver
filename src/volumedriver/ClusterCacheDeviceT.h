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

#ifndef CLUSTER_CACHE_DEVICE_T_
#define CLUSTER_CACHE_DEVICE_T_

#include "ClusterCacheDeviceManagerT.h"
#include "ClusterCacheEntry.h"
#include "Types.h"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mount.h>

#include <list>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>
#include <youtils/RWLock.h>
#include <youtils/Serialization.h>

namespace volumedriver
{

template<typename StoreType>
class ClusterCacheDeviceT
{
    friend class ClusterCacheDeviceManagerT<ClusterCacheDeviceT>;

public:
    ClusterCacheDeviceT(const fs::path& path,
                        const uint64_t size,
                        const size_t csize)
        : store_(path,
                 size,
                 csize)
        , entries_reloaded_(std::numeric_limits<uint64_t>::max())
    {
        ASSERT(store_.total_size() / cluster_size() < memory_.max_size());
        memory_.reserve(store_.total_size() / cluster_size());
    }

    ClusterCacheDeviceT()
        : entries_reloaded_(0)
    {}

    ClusterCacheDeviceT(const ClusterCacheDeviceT&) = delete;

    ClusterCacheDeviceT&
    operator=(const ClusterCacheDeviceT&) = delete;


    ssize_t
    read(uint8_t* buf,
         ClusterCacheEntry* entry)
    {
        return store_.read(buf,
                           getIndex(entry));
    }

    ssize_t
    write(const uint8_t* buf,
          ClusterCacheEntry* entry)
    {
        return store_.write(buf,
                            getIndex(entry));
    }

    void
    sync()
    {
        store_.sync();
    }

    bool
    hasEntry(const ClusterCacheEntry* entry) const
    {
        return (entry >= &memory_[0] and
                (uint64_t)(entry - &memory_[0]) < memory_.size());
    }

    bool
    hasPath(const fs::path& path)
    {
        return store_.path() == path;
    }

    ClusterCacheEntry*
    getEntry(const uint32_t index)
    {
        VERIFY(index < memory_.size());
        return &memory_[index];
    }

    uint32_t
    getIndex(const ClusterCacheEntry* entry) const
    {
        ASSERT(entry >= &memory_[0]);
        uint32_t index = entry - &memory_[0];
        VERIFY(index < memory_.size());
        return index;
    }

    void
    write_guid(const UUID& uuid) throw()
    {
        store_.write_guid(uuid);
    }

    struct ClusterCacheDeviceInfo
    {
        ClusterCacheDeviceInfo(fs::path ipath,
                            uint64_t itotal_size,
                            uint64_t iused_size)
        : path(ipath),
          total_size(itotal_size),
          used_size(iused_size)
        {}

        const fs::path path;
        const uint64_t total_size;
        const uint64_t used_size;
    };

    typedef ClusterCacheDeviceInfo Info;

    Info
    info() const
    {
        return Info(store_.path(),
                    store_.total_size(),
                    memory_.size() * cluster_size());
    }

    bool
    check_guid(const UUID& uuid) throw()
    {
        return store_.check_guid(uuid);
    }

    ClusterCacheEntry*
    getNextFreeCluster(const ClusterCacheKey& key)
    {
        if(memory_.size() * cluster_size() < store_.total_size())
        {
            // ret = &(memory_[used_size_/cluster_size_]);
            // used_size_ += cluster_size_;
            memory_.emplace_back(ClusterCacheEntry(key,
                                                   ClusterCacheMode::ContentBased));
            return &memory_.back();
        }
        return nullptr;
    }

    void
    reinstate()
    {
        store_.reinstate();
    }

    void
    check(const ClusterCacheEntry& e)
    {
        if (e.mode() == ClusterCacheMode::ContentBased)
        {
            store_.check(e.key.weed(),
                         getIndex(&e));
        }
    }

    // For Testing
    void
    fail_fd_forread()
    {
        store_.fail_fd_forread();
    }

    void
    fail_fd_forwrite()
    {
        store_.fail_fd_forwrite();
    }

    size_t
    cluster_size() const
    {
        return store_.cluster_size();
    }

    static size_t
    default_cluster_size()
    {
        return StoreType::default_cluster_size();
    }

private:
    DECLARE_LOGGER("ClusterCacheDevice");
    // what is called total_size is in fact clustersize smaller than available.

    StoreType store_;
    std::vector<ClusterCacheEntry> memory_;
    static const uint64_t test_frequency_ = 8192;
    uint64_t entries_reloaded_;

    friend class boost::serialization::access;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if(version != 0)
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }

        ar & store_;
        uint32_t used_size;
        ar & used_size;

        memory_.reserve(store_.total_size() / cluster_size());
        youtils::Weed w;
        for(size_t i = 0; i < used_size; ++i)
        {
            memory_.emplace_back(w);
        }
    }

    template<class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        if(version != 0)
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }

        ar & store_;
        uint32_t used_size = memory_.size();
        ar & used_size;
    }
};

}

#endif  // CLUSTER_CACHE_DEVICE_T_

// Local Variables: **
// mode: c++ **
// End: **
