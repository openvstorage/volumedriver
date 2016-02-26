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

#ifndef CLUSTER_CACHE_DEVICE_MANAGER_T_H_
#define CLUSTER_CACHE_DEVICE_MANAGER_T_H_

#include "ClusterCacheEntry.h"
#include "MountPointConfig.h"
#include "Types.h"
#include "VolumeConfig.h"

#include <list>
#include <map>
#include <vector>

#include <boost/serialization/vector.hpp>

#include <youtils/IOException.h>
#include <youtils/Serialization.h>
#include <youtils/UUID.h>

namespace volumedrivertest
{
class ClusterCacheTest;
}

namespace volumedriver
{

template<typename T>
class ClusterCacheDeviceManagerT
{

public:
    typedef T ManagedType;

    class ClusterCacheDeviceOutMapper
    {
        typedef ClusterCacheDeviceManagerT ManagerType;

    public:
        explicit ClusterCacheDeviceOutMapper(const ClusterCacheDeviceManagerT& man)
            : man_(man)
        {}

        uint32_t
        lookup(const T* dev) const
        {
            VERIFY(map_.find(dev) != map_.end());
            return map_.find(dev)->second;
        }

    private:
        friend class boost::serialization::access;

        BOOST_SERIALIZATION_SPLIT_MEMBER();

        template<class Archive>
        void
        save(Archive& ar, const unsigned int version) const
        {
            if(version != 0)
            {
                THROW_SERIALIZATION_ERROR(version, 0, 0);
            }

            std::vector<std::string> paths;

            for(const auto& dev : man_.devices)
            {
                typename T::Info info = dev->info();
                map_[dev.get()] = paths.size();
                paths.push_back(info.path.string());
            }
            ar & paths;
        }

        mutable std::map<const T*, uint32_t> map_;
        const ManagerType& man_;
    };

    class ClusterCacheDeviceInMapper
    {
    public:
        explicit ClusterCacheDeviceInMapper(ClusterCacheDeviceManagerT& man)
            : man_(man)
        {}

        T*
        lookup(const uint32_t num) const
        {
            VERIFY(map_.find(num) != map_.end());
            return map_.find(num)->second;
        }

        void
        remove(uint32_t num)
        {
            map_[num] = 0;
        }

    private:
        BOOST_SERIALIZATION_SPLIT_MEMBER();

        friend class boost::serialization::access;

        template<class Archive>
        void
        load(Archive& ar, const unsigned int version)
        {
            if(version != 0)
            {
                THROW_SERIALIZATION_ERROR(version, 0, 0);
            }

            std::vector<std::string> paths;
            ar & paths;

            for(size_t i = 0; i < paths.size(); ++i)
            {
                T* dev = man_.getDeviceFromPath(paths[i]);
                map_[i] = dev;
                if(dev)
                {
                    LOG_INFO("Adding " << paths[i] << " to restart KAC");
                }
                else
                {
                    LOG_INFO("Not adding " << paths[i] << " to restart KAC");
                }
            }
        }

        DECLARE_LOGGER("ClusterCacheDeviceInMapper");

        std::map<uint32_t, T*> map_;
        ClusterCacheDeviceManagerT& man_;
    };

    void
    fail_fd_forread()
    {
        for (auto& d : devices)
        {
            d->fail_fd_forread();
        }
    }

    void
    fail_fd_forwrite()
    {
        for (auto& d : devices)
        {
            d->fail_fd_forwrite();
        }
    }

private:
    DECLARE_LOGGER("ClusterCacheDeviceManagerT");

    std::list<std::unique_ptr<T>> devices;
    typename decltype(devices)::iterator devices_iterator;

    bool full;
    mutable fungi::RWLock rwlock;
    size_t cluster_size_;
    UUID uuid_;

    friend class volumedrivertest::ClusterCacheTest;
    friend class boost::serialization::access;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        ar & full;
        size_t size = 0;
        ar & size;

        youtils::UUID old_uuid;
        if(version >= 1)
        {
            ar & old_uuid;
        }

        if (version >= 2)
        {
            ar & cluster_size_;
        }
        else
        {
            cluster_size_ = ManagedType::default_cluster_size();
        }

        uuid_ = UUID();

        for(unsigned i = 0; i < size; ++i)
        {
            auto rd(std::make_unique<ManagedType>());
            ar & *rd;

            try
            {
                if (rd->cluster_size() != cluster_size_)
                {
                    LOG_ERROR(rd->info().path << ": cluster size changed? Was: " <<
                              rd->cluster_size() << ", now: " << cluster_size_);
                }
                else
                {
                    rd->reinstate();

                    if (rd->check_guid(old_uuid))
                    {
                        rd->write_guid(uuid_);
                        devices.emplace_back(std::move(rd));
                        devices_iterator = devices.begin();
                    }
                    else
                    {
                        LOG_ERROR("Not adding device " << rd->info().path <<
                                  " beause the guids don't match");
                    }
                }
            }
            CATCH_STD_ALL_EWHAT({
                    LOG_ERROR("Failed to reinstate read cache device " <<
                              rd->info().path << ": " << EWHAT);
                });
        }

        // We reset the guid here for the new device

        for(auto& dev : devices)
        {
            dev->write_guid(uuid_);
        }
    }

    template<class Archive>
    void
    save(Archive& ar,
         const unsigned /* version */) const
    {
        ar & full;
        size_t size = devices.size();
        ar & size;
        ar & uuid_;
        ar & cluster_size_;

        for (auto& dev : devices)
        {
            if(dev->check_guid(uuid_))
            {
                ar & *dev;
            }
            else
            {
                LOG_ERROR("Device " << dev->info().path << " does not have a correct guid, not serializing it");
            }
        }
    }

public:
    using DeviceInfo = typename T::Info;
    typedef std::map<fs::path, DeviceInfo> Info;

    ClusterCacheDeviceManagerT(const std::vector<MountPointConfig>& paths,
                               size_t cluster_size)
        : devices_iterator(devices.begin())
        , full(true)
        , rwlock("ClusterCacheDeviceManager")
        , cluster_size_(cluster_size)
    {
        for (auto it = paths.begin();
             it != paths.end();
             ++it)
        {
            addDevice(it->path,
                      it->size);
        }
    }

    explicit ClusterCacheDeviceManagerT(const size_t cluster_size)
        : devices_iterator(devices.begin())
        , full(true)
        , rwlock("ClusterCacheDeviceManager")
        , cluster_size_(cluster_size)
    {}

    ~ClusterCacheDeviceManagerT() = default;

    size_t
    cluster_size() const
    {
        return cluster_size_;
    }

    void
    info(Info& result)
    {
        result.clear();
        fungi::ScopedReadLock l(rwlock);
        for (auto& dev : devices)
        {
            typename T::Info i = dev->info();
            result.insert(typename Info::value_type(i.path, i));
        }
    }

    bool
    isStillClusterCacheDevice(const ManagedType* device)
    {
        for (const auto& dev : devices)
        {
            if (dev.get() == device)
            {
                return true;
            }
        }

        return false;
    }

    bool
    addDevice(const fs::path p,
              const uint64_t size = 0)
    {
        fungi::ScopedWriteLock l(rwlock);

        auto dev(std::make_unique<ManagedType>(p,
                                               size,
                                               cluster_size_));

        uuid_ = UUID();
        for (auto& dev : devices)
        {
            dev->write_guid(uuid_);
        }

        if(dev->check_guid(uuid_))
        {
            LOG_ERROR("Not adding device " << p << " because it seems to be in use already ");
            return false;
        }
        else
        {
            dev->write_guid(uuid_);
            devices.emplace_back(std::move(dev));
            devices_iterator = devices.begin();
            full = false;
            LOG_INFO("Added device " << p);
            return true;
        }
    }

    T*
    getDeviceFromEntry(const ClusterCacheEntry* entry)
    {
        fungi::ScopedReadLock l(rwlock);
        for(auto& dev : devices)
        {
            if(dev->hasEntry(entry))
            {
                return dev.get();
            }
        }
        return nullptr;

    }

    const T*
    getDeviceFromEntry(const ClusterCacheEntry* entry) const
    {
        fungi::ScopedReadLock l(rwlock);
        for(auto& dev : devices)
        {
            if(dev->hasEntry(entry))
            {
                return dev.get();
            }
        }
        return nullptr;
    }


    const T*
    getDeviceFromPath(const fs::path& path) const
    {
        fungi::ScopedReadLock l(rwlock);
        for(auto& dev : devices)
        {
            if(dev->hasPath_(path))
            {
                return dev.get();
            }
        }
        return nullptr;
    }

    T*
    getDeviceFromPath(const fs::path& path)
    {
        fungi::ScopedReadLock l(rwlock);
        for(auto& dev : devices)
        {
            if(dev->hasPath(path))
            {
                return dev.get();
            }
        }
        return nullptr;
    }

    void
    removeDevice(T* device)
    {
        fungi::ScopedWriteLock l(rwlock);
        for(auto it = devices.begin(); it != devices.end(); ++it)
        {
            if(it->get() == device)
            {
                devices.erase(it);
                devices_iterator = devices.begin();
                LOG_INFO("Removed device " << device);
                return;
            }
        }

        LOG_ERROR("Passed a device that I couldn't find");
    }

    void
    clear()
    {
        devices.clear();
    }

    void
    updateIterator()
    {
        VERIFY(devices_iterator != devices.end());

        if(devices_iterator == devices.end() or
           ++devices_iterator == devices.end())
        {
            devices_iterator = devices.begin();
        }
    }

    uint64_t
    totalSizeInEntries() const
    {
        // NO LOCKING SHOULD BE CALLED FROM KAK CONSTRUCTOR
        uint64_t res = 0;

        for(const auto& dev : devices)
        {
            res += dev->info().total_size;
        }

        return res / cluster_size_;
    }

    ClusterCacheEntry*
    getNextFreeCluster(const ClusterCacheKey& key,
                       T*& device)
    {
        ClusterCacheEntry* entry = nullptr;

        // This is called in a locked context
        if(full)
        {
            return 0;
        }

        for(auto it = devices_iterator; it != devices.end(); ++it)
        {
            if((entry = (*it)->getNextFreeCluster(key)))
            {
                updateIterator();
                device = it->get();
                return entry;
            }
        }
        for(auto it = devices.begin(); it != devices_iterator; ++it)
        {
            if((entry = (*it)->getNextFreeCluster(key)))
            {
                updateIterator();
                device = it->get();
                return entry;
            }
        }
        full = true;
        return 0;
    }

    void
    sync()
    {
        for (auto& dev : devices)
        {
            dev->sync();
        }
    }
};

}

#endif // !CLUSTER_CACHE_DEVICE_MANAGER_T_H_

// Local Variables: **
// mode: c++ **
// End: **
