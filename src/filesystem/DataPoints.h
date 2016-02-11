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

#ifndef VFS_DATA_POINT_H_
#define VFS_DATA_POINT_H_

#include <iostream>

#include <volumedriver/Api.h>
#include <volumedriver/PerformanceCounters.h>

namespace volumedriverfs
{

struct ClusterCacheDataPoint
{
    static constexpr const char* name = "cluster_cache";

    std::string id;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t entries;

    ClusterCacheDataPoint();
};

std::ostream&
operator<<(std::ostream&,
           const ClusterCacheDataPoint&);

struct ClusterCacheDeviceDataPoint
{
    static constexpr const char* name = "cluster_cache_device";

    std::string id;
    uint64_t used_bytes;
    uint64_t max_bytes;

    explicit ClusterCacheDeviceDataPoint(const volumedriver::ClusterCache::ManagerType::DeviceInfo&);
};

std::ostream&
operator<<(std::ostream&,
           const ClusterCacheDeviceDataPoint&);

struct ClusterCacheNamespaceDataPoint
{
    static constexpr const char* name = "cluster_cache_namespace";

    std::string id;
    uint64_t used_entries;
    uint64_t max_entries;

    explicit ClusterCacheNamespaceDataPoint(const volumedriver::ClusterCacheHandle&);
};

std::ostream&
operator<<(std::ostream&,
           const ClusterCacheNamespaceDataPoint&);

struct SCOCacheDeviceDataPoint
{
    static constexpr const char* name = "sco_cache_device";

    std::string id;
    uint64_t used_bytes;
    uint64_t max_bytes;

    explicit SCOCacheDeviceDataPoint(const volumedriver::SCOCacheMountPointInfo&);
};

std::ostream&
operator<<(std::ostream&,
           const SCOCacheDeviceDataPoint&);

struct SCOCacheNamespaceDataPoint
{
    static constexpr const char* name = "sco_cache_namespace";

    std::string id;
    uint64_t disposable_bytes;
    uint64_t non_disposable_bytes;
    uint64_t min_bytes;
    uint64_t max_non_disposable_bytes;

    explicit SCOCacheNamespaceDataPoint(const volumedriver::VolumeId&);
};

std::ostream&
operator<<(std::ostream&,
           const SCOCacheNamespaceDataPoint&);

struct VolumeMetaDataStoreDataPoint
{
    static constexpr const char* name = "volume_metadata_store";

    std::string id;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t used_clusters;
    uint64_t cached_pages;
    uint64_t max_pages;

    explicit VolumeMetaDataStoreDataPoint(const volumedriver::VolumeId&);
};

std::ostream&
operator<<(std::ostream&,
           const VolumeMetaDataStoreDataPoint&);

struct VolumeClusterCacheDataPoint
{
    static constexpr const char* name = "volume_cluster_cache";

    std::string id;
    uint64_t cache_hits;
    uint64_t cache_misses;

    explicit VolumeClusterCacheDataPoint(const volumedriver::VolumeId&);
};

std::ostream&
operator<<(std::ostream&,
           const VolumeClusterCacheDataPoint&);

struct VolumeSCOCacheDataPoint
{
    static constexpr const char* name = "volume_sco_cache";

    std::string id;
    uint64_t cache_hits;
    uint64_t cache_misses;

    explicit VolumeSCOCacheDataPoint(const volumedriver::VolumeId&);
};

std::ostream&
operator<<(std::ostream&,
           const VolumeSCOCacheDataPoint&);

struct VolumePerformanceCountersDataPoint
{
    static constexpr const char* name = "volume_performance_counters";

    std::string id;
    volumedriver::PerformanceCounters perf_counters;

    explicit VolumePerformanceCountersDataPoint(const volumedriver::VolumeId&);
};

std::ostream&
operator<<(std::ostream&,
           const VolumePerformanceCountersDataPoint&);

}

#endif // !VFS_DATA_POINT_H_
