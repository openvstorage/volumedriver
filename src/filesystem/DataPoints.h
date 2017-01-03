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

#ifndef VFS_DATA_POINT_H_
#define VFS_DATA_POINT_H_

#include <iostream>

#include <backend/ConnectionPool.h>

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

struct BackendConnectionPoolDataPoint
{
    static constexpr const char* name = "backend_connection_pool_counters";

    std::string id;
    size_t capacity;
    size_t size;
    backend::ConnectionPool::Counters counters;

    explicit BackendConnectionPoolDataPoint(const backend::ConnectionPool&);
};

std::ostream&
operator<<(std::ostream&,
           const BackendConnectionPoolDataPoint&);

}

#endif // !VFS_DATA_POINT_H_
