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

#ifndef VFS_PYTHON_CLIENT_H_
#define VFS_PYTHON_CLIENT_H_

#include "ClusterId.h"
#include "ClusterRegistry.h"
#include "XMLRPC.h"
#include "XMLRPCStructs.h"
#include "CloneFileFlags.h"
#include "ClientInfo.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/list.hpp>
#include <boost/python/tuple.hpp>
#include <boost/shared_ptr.hpp>

#include <youtils/ConfigurationReport.h>
#include <youtils/IOException.h>
#include <youtils/UpdateReport.h>

#include <volumedriver/MetaDataBackendConfig.h>
#include <volumedriver/FailOverCacheConfig.h>

namespace volumedriverfs
{

class LockedPythonClient;

namespace clienterrors
{

MAKE_EXCEPTION(PythonClientException, fungi::IOException);
MAKE_EXCEPTION(ObjectNotFoundException, fungi::IOException);
MAKE_EXCEPTION(InvalidOperationException, fungi::IOException);
MAKE_EXCEPTION(SnapshotNotFoundException, fungi::IOException);
MAKE_EXCEPTION(SnapshotNameAlreadyExistsException, fungi::IOException);
MAKE_EXCEPTION(FileExistsException, fungi::IOException);
MAKE_EXCEPTION(InsufficientResourcesException, fungi::IOException);
MAKE_EXCEPTION(PreviousSnapshotNotOnBackendException, fungi::IOException);
MAKE_EXCEPTION(ConfigurationUpdateException, fungi::IOException);
MAKE_EXCEPTION(NodeNotReachableException, fungi::IOException);
MAKE_EXCEPTION(ClusterNotReachableException, fungi::IOException);
MAKE_EXCEPTION(ObjectStillHasChildrenException, fungi::IOException);

class MaxRedirectsExceededException
    : public PythonClientException
{
public:
    MaxRedirectsExceededException(const char* method,
                                  const std::string& host,
                                  const uint16_t port)
       : PythonClientException(method)
       , method(method)
       , host(host)
       , port(port)
    {}

    const std::string method;
    const std::string host;
    const uint16_t port;
};

}

struct ClusterContact
{
    ClusterContact(const std::string& host,
                   const uint16_t port)
        : host(host)
        , port(port)
    {}

    std::string host;
    uint16_t port;
};

class PythonClient
{
private:
    static constexpr unsigned max_redirects_default = 2;

public:
    using Seconds = boost::chrono::seconds;
    using MaybeSeconds = boost::optional<Seconds>;

    // max_redirects should only be changed from its default in the testers
    // or by the LocalPythonClient

    PythonClient(const std::string& cluster_id,
                 const std::vector<ClusterContact>& cluster_contacts,
                 const MaybeSeconds& timeout = boost::none,
                 const unsigned max_redirects = max_redirects_default);

    virtual ~PythonClient() = default;

    std::vector<std::string>
    list_volumes(const boost::optional<std::string>& node_id = boost::none);

    std::vector<std::string>
    list_volumes_by_path();

    std::vector<std::string>
    list_snapshots(const std::string& volume_id);

    XMLRPCSnapshotInfo
    info_snapshot(const std::string& volume_id,
                  const std::string& snapshot_id);

    XMLRPCVolumeInfo
    info_volume(const std::string& volume_id);

    XMLRPCStatistics
    statistics_volume(const std::string& volume_id,
                      bool reset = false);

    XMLRPCStatistics
    statistics_node(const std::string& node_id,
                    bool reset = false);

    std::string
    create_snapshot(const std::string& volume_id,
                    const std::string& snapshot_id = "",
                    const std::string& metadata = "");

    std::string
    create_volume(const std::string& target_path,
                  boost::shared_ptr<volumedriver::MetaDataBackendConfig> mdb_config = nullptr,
                  const youtils::DimensionedValue& = youtils::DimensionedValue(),
                  const std::string& node_id = "");

    std::string
    create_clone(const std::string& target_path,
                 boost::shared_ptr<volumedriver::MetaDataBackendConfig> mdb_config,
                 const std::string& parent_volume_id,
                 const std::string& parent_snap_id,
                 const std::string& node_id = "");

    void
    resize(const std::string& object_id,
           const youtils::DimensionedValue&);

    void
    unlink(const std::string& target_path);

    void
    update_metadata_backend_config(const std::string& volume_id,
                                   boost::shared_ptr<volumedriver::MetaDataBackendConfig> mdb_config);

    std::string
    create_clone_from_template(const std::string& target_path,
                               boost::shared_ptr<volumedriver::MetaDataBackendConfig> mdb_config,
                               const std::string& parent_volume_id,
                               const std::string& node_id = "");

    void
    rollback_volume(const std::string& volume_id,
                    const std::string& snapshot_id);

    void
    delete_snapshot(const std::string& volume_id,
                    const std::string& snapshot_id);

    void
    set_volume_as_template(const std::string& vname);

    std::vector<std::string>
    get_scrubbing_work(const std::string& volume_id);

    void
    apply_scrubbing_result(const boost::python::tuple& tuple);

    void
    apply_scrubbing_result(const std::string& volume_id,
                           const std::string& scrub_res);

    std::string
    server_revision();

    std::string
    client_revision();

    void
    migrate(const std::string& object_id,
            const std::string& node_id,
            bool force = false);

    void
    mark_node_offline(const std::string& node_id);

    void
    mark_node_online(const std::string& node_id);

    uint64_t
    volume_potential(const std::string& node_id);

    std::map<NodeId, ClusterNodeStatus::State>
    info_cluster();

    boost::optional<ObjectId>
    get_object_id(const std::string& path);

    boost::optional<volumedriver::VolumeId>
    get_volume_id(const std::string& path);

    boost::python::dict
    get_sync_ignore(const std::string& volume_id);


    void
    set_sync_ignore(const std::string& volume_id,
                    uint64_t number_of_syncs_to_ignore,
                    uint64_t maximum_time_to_ignore_syncs_in_seconds);

    uint32_t
    get_sco_multiplier(const std::string& volume_id);


    void
    set_sco_multiplier(const std::string& volume_id,
                       uint32_t sco_multiplier);

    boost::optional<uint32_t>
    get_tlog_multiplier(const std::string& volume_id);


    void
    set_tlog_multiplier(const std::string& volume_id,
                        const boost::optional<uint32_t>& tlog_multiplier);

    boost::optional<float>
    get_sco_cache_max_non_disposable_factor(const std::string& volume_id);


    void
    set_sco_cache_max_non_disposable_factor(const std::string& volume_id,
                                            const boost::optional<float>& max_non_disposable_factor);

    FailOverCacheConfigMode
    get_failover_cache_config_mode(const std::string& volume_id);

    boost::optional<volumedriver::FailOverCacheConfig>
    get_failover_cache_config(const std::string& volume_id);

    void
    set_manual_failover_cache_config(const std::string& volume_id,
                                     const boost::optional<volumedriver::FailOverCacheConfig>& foc_config);

    void
    set_automatic_failover_cache_config(const std::string& volume_id);

    boost::optional<volumedriver::ClusterCacheBehaviour>
    get_cluster_cache_behaviour(const std::string& volume_id);

    void
    set_cluster_cache_behaviour(const std::string& volume_id,
                                const boost::optional<volumedriver::ClusterCacheBehaviour>&);

    boost::optional<volumedriver::ClusterCacheMode>
    get_cluster_cache_mode(const std::string& volume_id);

    void
    set_cluster_cache_mode(const std::string& volume_id,
                           const boost::optional<volumedriver::ClusterCacheMode>&);

    void
    set_cluster_cache_limit(const std::string& volume_id,
                            const boost::optional<volumedriver::ClusterCount>&
                            cluster_cache_limit);

    boost::optional<volumedriver::ClusterCount>
    get_cluster_cache_limit(const std::string& volume_id);

    void
    update_cluster_node_configs(const std::string& node_id);

    void
    vaai_copy(const std::string& src_path,
              const std::string& target_path,
              const uint64_t& timeout,
              const CloneFileFlags& flags);

    boost::shared_ptr<LockedPythonClient>
    make_locked_client(const std::string& volume_id,
                       const unsigned update_interval_secs = 3,
                       const unsigned grace_period_secs = 5);

    volumedriver::TLogName
    schedule_backend_sync(const std::string& volume_id);

    bool
    is_volume_synced_up_to_tlog(const std::string& volume_id,
                                const volumedriver::TLogName&);

    bool
    is_volume_synced_up_to_snapshot(const std::string& volume_id,
                                    const std::string& snapshot_name);

    boost::optional<size_t>
    get_metadata_cache_capacity(const std::string& volume_id);

    void
    set_metadata_cache_capacity(const std::string& volume_id,
                                const boost::optional<size_t>& num_pages);

    void
    stop_object(const std::string& id,
                bool delete_local_data = true);

    void
    restart_object(const std::string& id,
                   bool force_restart);

    std::vector<ClientInfo>
    list_client_connections(const std::string& node_id);

    const MaybeSeconds&
    timeout() const
    {
        return timeout_;
    }

protected:
    PythonClient(const MaybeSeconds& timeout)
        : timeout_(timeout)
        , max_redirects(max_redirects_default)
    {}

    XmlRpc::XmlRpcValue
    call(const char* method,
         XmlRpc::XmlRpcValue& req);

    XmlRpc::XmlRpcValue
    redirected_xmlrpc(const std::string& addr,
                      const uint16_t port,
                      const char* method,
                      XmlRpc::XmlRpcValue& req,
                      unsigned& redirect_count);

    std::string cluster_id_;
    std::mutex lock_;
    //this list does not necessarily contain all nodes in the cluster
    std::vector<ClusterContact> cluster_contacts_;
    const MaybeSeconds timeout_;

private:
    DECLARE_LOGGER("PythonClient");

public:
    const unsigned max_redirects;
};

}

#endif // !VFS_PYTHON_CLIENT_H_
