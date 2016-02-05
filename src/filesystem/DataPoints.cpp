// Copyright 2016 iNuron NV
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

#include "DataPoints.h"

#include <volumedriver/Api.h>
#include <volumedriver/VolManager.h>

namespace volumedriverfs
{

namespace be = backend;
namespace vd = volumedriver;

namespace
{

template<typename T>
std::ostream&
name_and_id(std::ostream& os,
            const T& val)
{
    return os << "name=" << val.name << ",id=" << val.id;
}

}

#define LOCKVD()                                                        \
    std::lock_guard<fungi::Mutex> vdg__(api::getManagementMutex())

ClusterCacheDataPoint::ClusterCacheDataPoint()
{
    api::getClusterCacheStats(cache_hits,
                              cache_misses,
                              entries);
}

constexpr const char* ClusterCacheDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const ClusterCacheDataPoint& ccc)
{
    return
        name_and_id(os, ccc) <<
        ",cache_hits=" << ccc.cache_hits <<
        ",cache_misses=" << ccc.cache_misses <<
        ",entries=" << ccc.entries;
}

ClusterCacheDeviceDataPoint::ClusterCacheDeviceDataPoint(const vd::ClusterCache::ManagerType::DeviceInfo& info)
    : id(info.path.string())
    , used_bytes(info.used_size)
    , max_bytes(info.total_size)
{}

constexpr const char* ClusterCacheDeviceDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const ClusterCacheDeviceDataPoint& cdc)
{
    return
        name_and_id(os, cdc) <<
        ",used_bytes=" << cdc.used_bytes <<
        ",max_bytes=" << cdc.max_bytes;
}

ClusterCacheNamespaceDataPoint::ClusterCacheNamespaceDataPoint(const vd::ClusterCacheHandle& h)
    : id(boost::lexical_cast<std::string>(h))
{
    vd::ClusterCache& cc = vd::VolManager::get()->getClusterCache();
    const vd::ClusterCache::NamespaceInfo info(cc.namespace_info(h));
    used_entries = info.entries;
    max_entries = info.max_entries ? *info.max_entries : 0;
}

constexpr const char* ClusterCacheNamespaceDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const ClusterCacheNamespaceDataPoint& cnc)
{
    return
        name_and_id(os, cnc) <<
        ",used_entries=" << cnc.used_entries <<
        ",max_entries=" << cnc.max_entries;
}

SCOCacheDeviceDataPoint::SCOCacheDeviceDataPoint(const vd::SCOCacheMountPointInfo& info)
    : id(info.path.string())
    , used_bytes(info.used)
    , max_bytes(info.capacity)
{}

constexpr const char* SCOCacheDeviceDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const SCOCacheDeviceDataPoint& sdc)
{
    return
        name_and_id(os, sdc) <<
        ",used_bytes=" << sdc.used_bytes <<
        ",max_bytes=" << sdc.max_bytes;
}

SCOCacheNamespaceDataPoint::SCOCacheNamespaceDataPoint(const vd::VolumeId& vid)
    : id(vid.str())
{
    const be::Namespace ns(id);

    LOCKVD();

    const vd::SCOCacheNamespaceInfo i(api::getVolumeSCOCacheInfo(ns));
    disposable_bytes = i.disposable;
    non_disposable_bytes = i.nondisposable;
    min_bytes = i.min;
    max_non_disposable_bytes = i.max_non_disposable;
}

constexpr const char* SCOCacheNamespaceDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const SCOCacheNamespaceDataPoint& snc)
{
    return
        name_and_id(os, snc) <<
        ",disposable_bytes=" << snc.disposable_bytes <<
        ",non_disposable_bytes=" << snc.non_disposable_bytes <<
        ",min_bytes=" << snc.min_bytes <<
        ",max_non_disposable_bytes=" << snc.max_non_disposable_bytes;
}

VolumeMetaDataStoreDataPoint::VolumeMetaDataStoreDataPoint(const vd::VolumeId& vid)
    : id(vid.str())
{
    LOCKVD();

    const vd::MetaDataStoreStats m(api::getMetaDataStoreStats(vid));

    cache_hits = m.cache_hits;
    cache_misses = m.cache_misses;
    used_clusters = m.used_clusters;
    cached_pages = m.cached_pages;
    max_pages = m.max_pages;
}

constexpr const char* VolumeMetaDataStoreDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const VolumeMetaDataStoreDataPoint& vmc)
{
    return
        name_and_id(os, vmc) <<
        ",cache_hits=" << vmc.cache_hits <<
        ",cache_misses=" << vmc.cache_misses <<
        ",used_clusters=" << vmc.used_clusters <<
        ",cached_pages=" << vmc.cached_pages <<
        ",max_pages=" << vmc.max_pages;
}

VolumeClusterCacheDataPoint::VolumeClusterCacheDataPoint(const vd::VolumeId& vid)
    : id(vid.str())
{
    LOCKVD();

    cache_hits = api::getClusterCacheHits(vid);
    cache_misses = api::getClusterCacheMisses(vid);
}

constexpr const char* VolumeClusterCacheDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const VolumeClusterCacheDataPoint& vcc)
{
    return
        name_and_id(os, vcc) <<
        ",cache_hits=" << vcc.cache_hits <<
        ",cache_misses=" << vcc.cache_misses;
};

VolumeSCOCacheDataPoint::VolumeSCOCacheDataPoint(const vd::VolumeId& vid)
    : id(vid)
{
    LOCKVD();
    cache_hits = api::getCacheHits(vid);
    cache_misses = api::getCacheMisses(vid);
}

constexpr const char* VolumeSCOCacheDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const VolumeSCOCacheDataPoint& vsc)
{
    return
        name_and_id(os, vsc) <<
        ",cache_hits=" << vsc.cache_hits <<
        ",cache_misses=" << vsc.cache_misses;
};

}
