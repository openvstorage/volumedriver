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

#ifndef VFS_XMLRPC_STRUCTS_H_
#define VFS_XMLRPC_STRUCTS_H_

#include "ObjectRegistration.h"

#include <iostream>
#include <string>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>

#include <youtils/Serialization.h>

#include <xmlrpc++0.7/src/XmlRpcValue.h>

#include <volumedriver/ClusterCacheHandle.h>
#include <volumedriver/ClusterCount.h>
#include <volumedriver/MetaDataBackendConfig.h>
#include <volumedriver/OwnerTag.h>
#include <volumedriver/PerformanceCounters.h>

namespace volumedriverfs
{

struct
XMLRPCStructs
{
    typedef boost::archive::binary_iarchive iarchive_type;
    typedef boost::archive::binary_oarchive oarchive_type;

    template<typename T>
    static ::XmlRpc::XmlRpcValue
    serialize_to_xmlrpc_value(const T& the_value)
    {
        std::stringstream ss;
        oarchive_type oa(ss);
        oa << the_value;

        return ::XmlRpc::XmlRpcValue(static_cast<const void*>(ss.str().c_str()),
                                     ss.str().size());
    }

    template<typename T>
    static T
    deserialize_from_xmlrpc_value(XmlRpc::XmlRpcValue& xval)
    {
        T return_value;
        std::stringstream ss;
        XmlRpc::XmlRpcValue::BinaryData data = xval;
        ss.str(std::string(data.begin(), data.end()));

        iarchive_type ia(ss);
        ia >> return_value;
        return return_value;
    }
};

struct XMLRPCVolumeInfo
{
    typedef boost::archive::xml_oarchive oarchive_type;
    typedef boost::archive::xml_iarchive iarchive_type;

    std::string volume_id;
    std::string _namespace_; //_ to avoid collision with keyword, is stripped off again in python binding
    std::string parent_namespace;
    std::string parent_snapshot_id;
    uint64_t volume_size;
    uint64_t lba_size;
    uint64_t lba_count;
    uint64_t cluster_multiplier;
    uint64_t sco_multiplier;
    boost::optional<uint64_t> tlog_multiplier;
    boost::optional<float> max_non_disposable_factor;
    std::string failover_mode;
    std::string failover_ip;
    uint16_t failover_port;
    bool halted;
    uint64_t footprint;
    uint64_t stored;
    ObjectType object_type;
    std::string parent_volume_id;
    std::string vrouter_id;
    // AR: revisit: we use unique_ptr throughout except when interfacing with boost::python
    boost::shared_ptr<volumedriver::MetaDataBackendConfig> metadata_backend_config;
    volumedriver::OwnerTag owner_tag =
        volumedriver::OwnerTag(0);
    volumedriver::ClusterCacheHandle cluster_cache_handle =
        volumedriver::ClusterCacheHandle(0);
    boost::optional<volumedriver::ClusterCount> cluster_cache_limit;

    bool
    operator==(const XMLRPCVolumeInfo& other) const
    {
#define EQ(x)                                   \
    (x == other.x)

        return
            EQ(volume_id) and
            EQ(_namespace_) and
            EQ(parent_namespace) and
            EQ(parent_snapshot_id) and
            EQ(volume_size) and
            EQ(lba_size) and
            EQ(lba_count) and
            EQ(cluster_multiplier) and
            EQ(sco_multiplier) and
            EQ(tlog_multiplier) and
            EQ(max_non_disposable_factor) and
            EQ(failover_mode) and
            EQ(failover_ip) and
            EQ(failover_port) and
            EQ(halted) and
            EQ(footprint) and
            EQ(stored) and
            EQ(object_type) and
            EQ(parent_volume_id) and
            EQ(vrouter_id) and
            EQ(metadata_backend_config) and
            EQ(owner_tag) and
            EQ(cluster_cache_handle) and
            EQ(cluster_cache_limit);

#undef EQ
    }


    friend class boost::serialization::access;

    template<typename Archive>
    void
    serialize(Archive& ar, const unsigned int version)
    {
        ar & BOOST_SERIALIZATION_NVP(volume_id);
        ar & BOOST_SERIALIZATION_NVP(_namespace_);
        ar & BOOST_SERIALIZATION_NVP(parent_namespace);
        ar & BOOST_SERIALIZATION_NVP(parent_snapshot_id);
        ar & BOOST_SERIALIZATION_NVP(volume_size);
        ar & BOOST_SERIALIZATION_NVP(lba_size);
        ar & BOOST_SERIALIZATION_NVP(lba_count);
        ar & BOOST_SERIALIZATION_NVP(cluster_multiplier);
        ar & BOOST_SERIALIZATION_NVP(sco_multiplier);
        ar & BOOST_SERIALIZATION_NVP(tlog_multiplier);
        ar & BOOST_SERIALIZATION_NVP(max_non_disposable_factor);
        ar & BOOST_SERIALIZATION_NVP(failover_mode);
        ar & BOOST_SERIALIZATION_NVP(failover_ip);
        ar & BOOST_SERIALIZATION_NVP(failover_port);
        ar & BOOST_SERIALIZATION_NVP(halted);
        ar & BOOST_SERIALIZATION_NVP(footprint);
        ar & BOOST_SERIALIZATION_NVP(stored);
        ar & BOOST_SERIALIZATION_NVP(object_type);
        ar & BOOST_SERIALIZATION_NVP(parent_volume_id);
        ar & BOOST_SERIALIZATION_NVP(vrouter_id);
        ar & BOOST_SERIALIZATION_NVP(metadata_backend_config);

        if (version > 1)
        {
            ar & BOOST_SERIALIZATION_NVP(owner_tag);
            ar & BOOST_SERIALIZATION_NVP(cluster_cache_handle);
            ar & BOOST_SERIALIZATION_NVP(cluster_cache_limit);
        }
    }

    static constexpr const char* serialization_name =  "XMLRPCVolumeInfo";

    void
    set_from_str(const std::string& str);

    std::string
    str() const;
};

// No reference to "volume" in XMLRPCStatistics structname or python classname as we
// could reuse this struct for reporting aggregate values (per VSA / vPool)
struct XMLRPCStatistics
{
    typedef boost::archive::xml_oarchive oarchive_type;
    typedef boost::archive::xml_iarchive iarchive_type;

    uint64_t sco_cache_hits = 0;
    uint64_t sco_cache_misses = 0;
    uint64_t cluster_cache_hits = 0;
    uint64_t cluster_cache_misses = 0;
    uint64_t metadata_store_hits = 0;
    uint64_t metadata_store_misses = 0;
    uint64_t stored = 0;
    uint64_t partial_read_fast = 0;
    uint64_t partial_read_slow = 0;

    volumedriver::PerformanceCounters performance_counters;

    bool
    operator==(const XMLRPCStatistics& other) const
    {
#define EQ(x)                                   \
        (x == other.x)
        return
            EQ(sco_cache_hits) and
            EQ(sco_cache_misses) and
            EQ(cluster_cache_hits) and
            EQ(cluster_cache_misses) and
            EQ(metadata_store_hits) and
            EQ(metadata_store_misses) and
            EQ(stored) and
            EQ(partial_read_fast) and
            EQ(partial_read_slow) and
            EQ(performance_counters)
            ;
#undef EQ
    }

    template<typename Archive>
    void
    serialize(Archive& ar, const unsigned int version)
    {
        ar & BOOST_SERIALIZATION_NVP(sco_cache_hits);
        ar & BOOST_SERIALIZATION_NVP(sco_cache_misses);
        ar & BOOST_SERIALIZATION_NVP(cluster_cache_hits);
        ar & BOOST_SERIALIZATION_NVP(cluster_cache_misses);
        ar & BOOST_SERIALIZATION_NVP(metadata_store_hits);
        ar & BOOST_SERIALIZATION_NVP(metadata_store_misses);
        ar & BOOST_SERIALIZATION_NVP(performance_counters);

        if (version > 1)
        {
            ar & BOOST_SERIALIZATION_NVP(stored);
        }

        if (version > 2)
        {
            ar & BOOST_SERIALIZATION_NVP(partial_read_fast);
            ar & BOOST_SERIALIZATION_NVP(partial_read_slow);
        }
    }

    static constexpr const char* serialization_name =  "XMLRPCStatistics";

    void
    set_from_str(const std::string& str);

    std::string
    str() const;
};

struct XMLRPCSnapshotInfo
{
    typedef boost::archive::xml_oarchive oarchive_type;
    typedef boost::archive::xml_iarchive iarchive_type;

    std::string snapshot_id;
    std::string timestamp;
    std::string uuid;
    uint64_t stored;
    bool in_backend;
    std::string metadata;

    XMLRPCSnapshotInfo(const std::string& i,
                       const std::string& t,
                       const std::string& u,
                       uint64_t s,
                       bool b,
                       const std::string& m)
        : snapshot_id(i)
        , timestamp(t)
        , uuid(u)
        , stored(s)
        , in_backend(b)
        , metadata(m)
    {}

    XMLRPCSnapshotInfo()
        : snapshot_id("__uninitialized__")
        , timestamp("__uninitialized__")
        , uuid("__uninitialized__")
        , stored(0)
        , in_backend(false)
        , metadata("__uninitialized__")
    {}

    ~XMLRPCSnapshotInfo() = default;

    XMLRPCSnapshotInfo(const XMLRPCSnapshotInfo&) = default;

    XMLRPCSnapshotInfo&
    operator=(const XMLRPCSnapshotInfo&) = default;

    bool
    operator==(const XMLRPCSnapshotInfo& other) const
    {
#define EQ(x)                                   \
        (x == other.x)

        return EQ(snapshot_id) and
            EQ(timestamp) and
            EQ(uuid) and
            EQ(stored) and
            EQ(in_backend) and
            EQ(metadata);

#undef EQ
    }

    template<typename Archive>
    void
    serialize(Archive& ar, const unsigned int version)
    {
        CHECK_VERSION(version, 2);

        ar & BOOST_SERIALIZATION_NVP(snapshot_id);
        ar & BOOST_SERIALIZATION_NVP(timestamp);
        ar & BOOST_SERIALIZATION_NVP(uuid);
        ar & BOOST_SERIALIZATION_NVP(stored);
        ar & BOOST_SERIALIZATION_NVP(in_backend);
        ar & BOOST_SERIALIZATION_NVP(metadata);
    }

    static constexpr const char* serialization_name = "XMLRPCSnapshotInfo";

    void
    set_from_str(const std::string& str);

    std::string
    str() const;
};

struct XMLRPCClusterCacheHandleInfo
{
    typedef boost::archive::xml_oarchive oarchive_type;
    typedef boost::archive::xml_iarchive iarchive_type;

    volumedriver::ClusterCacheHandle cluster_cache_handle;
    uint64_t entries;
    boost::optional<uint64_t> max_entries;
    std::vector<uint64_t> map_stats;

    XMLRPCClusterCacheHandleInfo(const volumedriver::ClusterCacheHandle hdl,
                                 const uint64_t cnt,
                                 const boost::optional<uint64_t> max,
                                 const std::vector<uint64_t>& stats)
        : cluster_cache_handle(hdl)
        , entries(cnt)
        , max_entries(max)
        , map_stats(stats)
    {}

    XMLRPCClusterCacheHandleInfo()
        : cluster_cache_handle(std::numeric_limits<uint64_t>::max())
        , entries(std::numeric_limits<uint64_t>::max())
    {}

    ~XMLRPCClusterCacheHandleInfo() = default;

    XMLRPCClusterCacheHandleInfo(const XMLRPCClusterCacheHandleInfo&) = default;

    XMLRPCClusterCacheHandleInfo&
    operator=(const XMLRPCClusterCacheHandleInfo&) = default;

    bool
    operator==(const XMLRPCClusterCacheHandleInfo& other) const
    {
#define EQ(x)                                   \
        (x == other.x)

        return EQ(cluster_cache_handle) and
            EQ(entries) and
            EQ(max_entries) and
            EQ(map_stats);
#undef EQ
    }

    template<typename Archive>
    void
    serialize(Archive& ar, const unsigned int version)
    {
        CHECK_VERSION(version, 1);

        ar & BOOST_SERIALIZATION_NVP(cluster_cache_handle);
        ar & BOOST_SERIALIZATION_NVP(entries);
        ar & BOOST_SERIALIZATION_NVP(max_entries);
        ar & BOOST_SERIALIZATION_NVP(map_stats);
    }

    static constexpr const char* serialization_name = "XMLRPCClusterCacheHandleInfo";

    void
    set_from_str(const std::string& str);

    std::string
    str() const;
};

}

BOOST_CLASS_VERSION(volumedriverfs::XMLRPCVolumeInfo, 2);
BOOST_CLASS_VERSION(volumedriverfs::XMLRPCStatistics, 3);
BOOST_CLASS_VERSION(volumedriverfs::XMLRPCSnapshotInfo, 2);
BOOST_CLASS_VERSION(volumedriverfs::XMLRPCClusterCacheHandleInfo, 1);

#endif // !VFS_XMLRPC_STRUCTS_H_
