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

VolumeBackendDataPoint::VolumeBackendDataPoint(const vd::VolumeId& vid)
    : id(vid)
{
    LOCKVD();
    partial_read_counter_ = api::getPartialReadCounter(vid);
}

constexpr const char* VolumeBackendDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const VolumeBackendDataPoint& dp)
{
    return
        name_and_id(os, dp) <<
        ",fast=" << dp.partial_read_counter_.fast <<
        ",slow=" << dp.partial_read_counter_.slow;
};

namespace
{

vd::PerformanceCounters
get_perf_counters(const vd::VolumeId& id)
{
    LOCKVD();

    vd::PerformanceCounters pc;
    std::swap(pc, api::performance_counters(id));
    return pc;
}

template<typename T, typename U>
std::ostream&
stream_perf_counter(std::ostream& os,
                    const vd::PerformanceCounter<T, U>& pc,
                    const char* pfx)
{
    os <<
        "," << pfx << "_events=" << pc.events() <<
        "," << pfx << "_sum=" << pc.sum() <<
        "," << pfx << "_sqsum=" << pc.sum_of_squares() <<
        "," << pfx << "_min=" << pc.min() <<
        "," << pfx << "_max=" << pc.max() <<
        "," << pfx << "_distribution={";

    bool fst = true;
    for (size_t i = 0; i < pc.bucket_bounds().size(); ++i)
    {
        if (fst)
        {
            fst = false;
        }
        else
        {
            os << ",";
        }
        os << pc.bucket_bounds()[i] << ":" << pc.buckets()[i];
    }

    os << "}";
    return os;
}

}

VolumePerformanceCountersDataPoint::VolumePerformanceCountersDataPoint(const vd::VolumeId& vid)
    : id(vid)
    , perf_counters(get_perf_counters(vid))
{}

constexpr const char* VolumePerformanceCountersDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const VolumePerformanceCountersDataPoint& dp)
{
        name_and_id(os, dp);

        stream_perf_counter(os,
                            dp.perf_counters.write_request_size,
                            "write_request_size");
        stream_perf_counter(os
                            , dp.perf_counters.write_request_usecs,
                            "write_request_usecs");
        stream_perf_counter(os,
                            dp.perf_counters.unaligned_write_request_size,
                            "unaligned_write_request_size");
        stream_perf_counter(os,
                            dp.perf_counters.backend_write_request_size,
                            "backend_write_request_size");
        stream_perf_counter(os,
                            dp.perf_counters.backend_write_request_usecs,
                            "backend_write_request_usecs");
        stream_perf_counter(os,
                            dp.perf_counters.read_request_size,
                            "read_request_size");
        stream_perf_counter(os,
                            dp.perf_counters.read_request_usecs,
                            "read_request_usecs");
        stream_perf_counter(os,
                            dp.perf_counters.unaligned_read_request_size,
                            "unaligned_read_request_size");
        stream_perf_counter(os,
                            dp.perf_counters.backend_read_request_size,
                            "backend_read_request_size");
        stream_perf_counter(os,
                            dp.perf_counters.backend_read_request_usecs,
                            "backend_read_request_usecs");
        stream_perf_counter(os,
                            dp.perf_counters.sync_request_usecs,
                            "sync_request_usecs");

        return os;
}

BackendConnectionPoolDataPoint::BackendConnectionPoolDataPoint(const backend::ConnectionPool& p)
    : id(boost::lexical_cast<std::string>(p.config()))
    , capacity(p.capacity())
    , size(p.size())
    , counters(p.counters())
{}

constexpr const char* BackendConnectionPoolDataPoint::name;

std::ostream&
operator<<(std::ostream& os,
           const BackendConnectionPoolDataPoint& dp)
{
    return
        name_and_id(os, dp) <<
        ",capacity=" << dp.capacity <<
        ",size=" << dp.size <<
        "," << dp.counters;
}

}
