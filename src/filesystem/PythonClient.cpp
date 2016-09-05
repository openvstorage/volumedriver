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

#include "FailOverCacheConfigMode.h"
#include "LockedPythonClient.h"
#include "PythonClient.h"
#include "XMLRPCKeys.h"
#include "XMLRPCUtils.h"

#include <boost/python/extract.hpp>
#include <boost/serialization/map.hpp>

#include <xmlrpc++0.7/src/XmlRpcClient.h>
#include <xmlrpc++0.7/src/XmlRpcValue.h>

#include <youtils/DimensionedValue.h>
#include <youtils/Logging.h>
#include <youtils/BuildInfoString.h>

#include <volumedriver/Snapshot.h>
#include <volumedriver/VolumeConfig.h>

namespace volumedriverfs
{

namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

#define LOCK()                                                  \
    std::lock_guard<decltype(lock_)> lg__(lock_)

PythonClient::PythonClient(const std::string& cluster_id,
                           const std::vector< ClusterContact >& cluster_contacts,
                           const boost::optional<boost::chrono::seconds>& timeout,
                           const unsigned max_redirects)
    : cluster_id_(cluster_id)
    , cluster_contacts_(cluster_contacts)
    , timeout_(timeout)
    , max_redirects(max_redirects)
{
}
static const uint64_t default_sco_size = vd::VolumeConfig::default_sco_size_;


// We don't maintain an XmlRpcClient as member variable as our server has the habit
// of closing the connection after each request, which confuses the client code
// (it'll try to reconnect but occasionally run into an EINPROGRESS error on
// the second request:
//  2013-11-13 11:53:42:757808 +0100 CET -- XmlRpcClient -- error -- handleEvent: Could not connect to servererror 115 -- [0x00007ffff7fd2740]
// ).
// Until that becomes a bottleneck and the underlying issue
// is fixed we'll create a new client instance for each xmlrpc() invocation.
XmlRpc::XmlRpcValue
PythonClient::redirected_xmlrpc(const std::string& addr,
                                const uint16_t port,
                                const char* method,
                                XmlRpc::XmlRpcValue& req,
                                unsigned& redirect_count)
{
    if (redirect_count > max_redirects)
    {
        LOG_ERROR(method << ": redirect limit of " << max_redirects <<
                  " exceeded - giving up");
        throw clienterrors::MaxRedirectsExceededException(method,
                                                          addr,
                                                          port);
    }

    req[XMLRPCKeys::vrouter_cluster_id] = cluster_id_;

#if 0
    std::cout << "request: ";
    req.write(std::cout);
    std::cout << std::endl;
#endif
    XmlRpc::XmlRpcValue rsp;

    XmlRpc::XmlRpcClient xclient(addr.c_str(), port);
    const bool res = xclient.execute(method,
                                     req,
                                     rsp,
                                     timeout_ ?
                                     timeout_->count() :
                                     -1.0);
    if (not res)
    {
        // Bummer, there does not seem to be a way to get at a precise error message.
        LOG_ERROR("Failed to send XMLRPC request " << method);
        throw clienterrors::NodeNotReachableException("failed to send XMLRPC request", method);
    }

    if (xclient.isFault())
    {
        LOG_ERROR("XMLRPC request " << method << " resulted in fault response");
        throw clienterrors::PythonClientException("got fault response", method);
    }

#if 0
    std::cout << "response: ";
    rsp.write(std::cout);
    std::cout << std::endl;
#endif

    // Does that duplicate the isFault() above?
    // Otherwise, move these to XMLRPCKeys as well?
    if (rsp.hasMember("faultCode"))
    {
        THROW_UNLESS(rsp.hasMember("faultString"));
        const std::string what(rsp["faultString"]);
        throw clienterrors::PythonClientException(method, what.c_str());
    }
    else if (rsp.hasMember(XMLRPCKeys::xmlrpc_error_code))
    {
        int errorcode_int = int(rsp[XMLRPCKeys::xmlrpc_error_code]);
        XMLRPCErrorCode errorcode = (XMLRPCErrorCode) errorcode_int;
        std::string errorstring = rsp.hasMember(XMLRPCKeys::xmlrpc_error_string) ?
                                  rsp[XMLRPCKeys::xmlrpc_error_string] :
                                  "";

        switch (errorcode)
        {
        case XMLRPCErrorCode::ObjectNotFound:
            throw clienterrors::ObjectNotFoundException(errorstring.c_str());
        case XMLRPCErrorCode::InvalidOperation:
            throw clienterrors::InvalidOperationException(errorstring.c_str());
        case XMLRPCErrorCode::SnapshotNotFound:
            throw clienterrors::SnapshotNotFoundException(errorstring.c_str());
        case XMLRPCErrorCode::FileExists:
            throw clienterrors::FileExistsException(errorstring.c_str());
        case XMLRPCErrorCode::InsufficientResources:
            throw clienterrors::InsufficientResourcesException(errorstring.c_str());
        case XMLRPCErrorCode::PreviousSnapshotNotOnBackend:
            throw clienterrors::PreviousSnapshotNotOnBackendException(errorstring.c_str());
        case XMLRPCErrorCode::ObjectStillHasChildren:
            throw clienterrors::ObjectStillHasChildrenException(errorstring.c_str());
        case XMLRPCErrorCode::SnapshotNameAlreadyExists:
            throw clienterrors::SnapshotNameAlreadyExistsException(errorstring.c_str());
        default:
            {
                //forward compatibility
                std::stringstream ss;
                ss << "received unknown error code " << errorcode_int << ": " << errorstring;
                throw clienterrors::PythonClientException(ss.str().c_str());
            }
        }
    }
    else if (rsp.hasMember(XMLRPCKeys::xmlrpc_redirect_host))
    {
        THROW_UNLESS(rsp.hasMember(XMLRPCKeys::xmlrpc_redirect_port));

        const std::string host(rsp[XMLRPCKeys::xmlrpc_redirect_host]);
        const std::string portstr(rsp[XMLRPCKeys::xmlrpc_redirect_port]);
        const uint16_t port = boost::lexical_cast<uint16_t>(portstr);

        return redirected_xmlrpc(host,
                                 port,
                                 method,
                                 req,
                                 ++redirect_count);
    }
    else
    {
        return rsp;
    }
}

XmlRpc::XmlRpcValue
PythonClient::call(const char* method,
                   XmlRpc::XmlRpcValue& req)
{
    unsigned redirect_counter = 0;

    std::vector<ClusterContact> cluster_contacts;

    {
        LOCK();
        cluster_contacts = cluster_contacts_;
    }

    for (const auto& contact : cluster_contacts)
    {
        try
        {
            return redirected_xmlrpc(contact.host,
                                     contact.port,
                                     method,
                                     req,
                                     redirect_counter);
        }
        catch(clienterrors::NodeNotReachableException& e)
        {
            if (redirect_counter > 0)
            {
                //we were able to contact at least one node in the cluster
                //so we don't throw a ClusterNotReachableException here
                throw;
            }
            else
            {
                LOCK();
                std::rotate(cluster_contacts_.begin(),
                            cluster_contacts_.begin() + 1,
                            cluster_contacts_.end());
            }
        }
    }

    throw clienterrors::ClusterNotReachableException("");
};

bpy::dict
PythonClient::get_sync_ignore(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    auto rsp(call(GetSyncIgnore::method_name(), req));
    bpy::dict the_dict;
    the_dict[XMLRPCKeys::number_of_syncs_to_ignore] =
        boost::lexical_cast<uint64_t>(static_cast<std::string>(rsp[XMLRPCKeys::number_of_syncs_to_ignore]));
    the_dict[XMLRPCKeys::maximum_time_to_ignore_syncs_in_seconds] =
        boost::lexical_cast<uint64_t>(static_cast<std::string>(rsp[XMLRPCKeys::maximum_time_to_ignore_syncs_in_seconds]));
    return the_dict;
}


void
PythonClient::set_sync_ignore(const std::string& volume_id,
                              const uint64_t number_of_syncs_to_ignore,
                              const uint64_t maximum_time_to_ignore_syncs_in_seconds)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    req[XMLRPCKeys::number_of_syncs_to_ignore] =
        boost::lexical_cast<std::string>(number_of_syncs_to_ignore);
    req[XMLRPCKeys::maximum_time_to_ignore_syncs_in_seconds] =
        boost::lexical_cast<std::string>(maximum_time_to_ignore_syncs_in_seconds);
    auto rsp(call(SetSyncIgnore::method_name(), req));
}

uint32_t
PythonClient::get_sco_multiplier(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    auto rsp(call(GetSCOMultiplier::method_name(), req));
    uint32_t val =
        boost::lexical_cast<uint32_t>(static_cast<std::string>(rsp[XMLRPCKeys::sco_multiplier]));
    return val;
}

void
PythonClient::set_sco_multiplier(const std::string& volume_id,
                                 const uint32_t sco_multiplier)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    req[XMLRPCKeys::sco_multiplier] =
        boost::lexical_cast<std::string>(sco_multiplier);
    auto rsp(call(SetSCOMultiplier::method_name(), req));
}

boost::optional<uint32_t>
PythonClient::get_tlog_multiplier(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    auto rsp(call(GetTLogMultiplier::method_name(), req));

    boost::optional<uint32_t> tm;
    if (rsp.hasMember(XMLRPCKeys::tlog_multiplier))
    {
        tm = boost::optional<uint32_t>(boost::lexical_cast<uint32_t>(static_cast<std::string>(rsp[XMLRPCKeys::tlog_multiplier])));
    }
    return tm;
}

void
PythonClient::set_tlog_multiplier(const std::string& volume_id,
                                  const boost::optional<uint32_t>& tlog_multiplier)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    if (tlog_multiplier)
    {
        req[XMLRPCKeys::tlog_multiplier] = boost::lexical_cast<std::string>(tlog_multiplier.get());
    }
    auto rsp(call(SetTLogMultiplier::method_name(), req));
}

boost::optional<float>
PythonClient::get_sco_cache_max_non_disposable_factor(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    auto rsp(call(GetSCOCacheMaxNonDisposableFactor::method_name(), req));

    boost::optional<float> scmndf;
    if (rsp.hasMember(XMLRPCKeys::max_non_disposable_factor))
    {
        scmndf = boost::optional<float>(boost::lexical_cast<float>(static_cast<std::string>(rsp[XMLRPCKeys::max_non_disposable_factor])));
    }
    return scmndf;
}


void
PythonClient::set_sco_cache_max_non_disposable_factor(const std::string& volume_id,
                                                      const boost::optional<float>& max_non_disposable_factor)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    if (max_non_disposable_factor)
    {
        req[XMLRPCKeys::max_non_disposable_factor] = boost::lexical_cast<std::string>(max_non_disposable_factor.get());
    }
    auto rsp(call(SetSCOCacheMaxNonDisposableFactor::method_name(), req));
}


FailOverCacheConfigMode
PythonClient::get_failover_cache_config_mode(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;

    auto rsp(call(GetFailOverCacheConfigMode::method_name(), req));
    std::string res = rsp[XMLRPCKeys::foc_config_mode];

    FailOverCacheConfigMode foc_cm;
    std::stringstream ss(res);
    ss >> foc_cm;

    return foc_cm;
}

boost::optional<vd::FailOverCacheConfig>
PythonClient::get_failover_cache_config(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    auto rsp(call(GetFailOverCacheConfig::method_name(), req));
    boost::optional<vd::FailOverCacheConfig> foc_config;
    if (rsp.hasMember(XMLRPCKeys::foc_config))
    {
        std::stringstream ss(rsp[XMLRPCKeys::foc_config]);
        boost::archive::text_iarchive ia(ss);
        vd::FailOverCacheConfig fc("",
                                   0,
                                   vd::FailOverCacheMode::Asynchronous);
        ia >> fc;
        foc_config = fc;
    }
    return foc_config;
}

void
PythonClient::set_manual_failover_cache_config(const std::string& volume_id,
                                               const boost::optional<vd::FailOverCacheConfig>& foc_config)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    if (foc_config)
    {
        std::stringstream ss;
        boost::archive::text_oarchive oa(ss);
        oa << *foc_config;
        req[XMLRPCKeys::foc_config] = ss.str();
    }
    auto rsp(call(SetManualFailOverCacheConfig::method_name(), req));
}

void
PythonClient::set_automatic_failover_cache_config(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    auto rsp(call(SetAutomaticFailOverCacheConfig::method_name(), req));
}

namespace
{

std::vector<std::string>
extract_vec(const XmlRpc::XmlRpcValue& rsp)
{
    std::vector<std::string> vec;
    vec.reserve(rsp.size());

    for (auto i = 0; i < rsp.size(); ++i)
    {
        // TODO: fix xmlrpc++ to support const overloads of the
        // conversion operators.
        std::string s = const_cast<XmlRpc::XmlRpcValue&>(rsp[i]);
        vec.emplace_back(std::move(s));
    }

    return vec;
}

}

std::vector<std::string>
PythonClient::list_volumes(const boost::optional<std::string>& node_id)
{
    XmlRpc::XmlRpcValue req;
    if (node_id)
    {
        req[XMLRPCKeys::vrouter_id] = *node_id;
    }

    return extract_vec(call(VolumesList::method_name(),
                            req));
}

std::vector<std::string>
PythonClient::list_volumes_by_path()
{
    XmlRpc::XmlRpcValue req;
    return extract_vec(call(VolumesListByPath::method_name(),
                            req));
}

std::vector<std::string>
PythonClient::get_scrubbing_work(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;

    return extract_vec(call(GetScrubbingWork::method_name(),
                            req));
}

void
PythonClient::apply_scrubbing_result(const bpy::tuple& tup)
{
    const std::string volume_id = bpy::extract<std::string>(tup[0]);
    const std::string work = bpy::extract<std::string>(tup[1]);

    apply_scrubbing_result(volume_id,
                           work);
}

void
PythonClient::apply_scrubbing_result(const std::string& volume_id,
                                     const std::string& scrub_res)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    req[XMLRPCKeys::scrubbing_work_result] = scrub_res;

    call(ApplyScrubbingResult::method_name(), req);
}

std::vector<std::string>
PythonClient::list_snapshots(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = volume_id;

    return extract_vec(call(SnapshotsList::method_name(),
                            req));
}

std::vector<ClientInfo>
PythonClient::list_client_connections(const std::string& node_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::vrouter_id] = node_id;

    auto rsp(call(ListClientConnections::method_name(), req));

    std::vector<ClientInfo> info;
    for (auto i = 0; i < rsp.size(); ++i)
    {
        info.push_back(XMLRPCStructs::deserialize_from_xmlrpc_value<ClientInfo>(rsp[i]));
    }
    return info;
}

XMLRPCSnapshotInfo
PythonClient::info_snapshot(const std::string& volume_id,
                            const std::string& snapshot_id)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = volume_id;
    req[XMLRPCKeys::snapshot_id] = snapshot_id;

    auto rsp(call(SnapshotInfo::method_name(), req));
    return XMLRPCStructs::deserialize_from_xmlrpc_value<XMLRPCSnapshotInfo>(rsp);
}

XMLRPCVolumeInfo
PythonClient::info_volume(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = volume_id;
    auto rsp(call(VolumeInfo::method_name(), req));
    return XMLRPCStructs::deserialize_from_xmlrpc_value<XMLRPCVolumeInfo>(rsp);
}

XMLRPCStatistics
PythonClient::statistics_volume(const std::string& volume_id,
                                bool reset)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = volume_id;
    XMLRPCUtils::put(req,
                     XMLRPCKeys::reset,
                     reset);

    auto rsp(call(VolumePerformanceCounters::method_name(), req));
    return XMLRPCStructs::deserialize_from_xmlrpc_value<XMLRPCStatistics>(rsp);
}

XMLRPCStatistics
PythonClient::statistics_node(const std::string& node_id,
                              bool reset)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::vrouter_id] = node_id;
    XMLRPCUtils::put(req,
                     XMLRPCKeys::reset,
                     reset);

    auto rsp(call(VolumeDriverPerformanceCounters::method_name(), req));
    return XMLRPCStructs::deserialize_from_xmlrpc_value<XMLRPCStatistics>(rsp);
}

boost::optional<vd::VolumeId>
PythonClient::get_volume_id(const std::string& path)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::target_path] = path;
    auto rsp(call(GetVolumeId::method_name(), req));

    const std::string id(rsp[XMLRPCKeys::volume_id]);
    if (id.empty())
    {
        return boost::none;
    }
    else
    {
        return vd::VolumeId(id);
    }
}

boost::optional<ObjectId>
PythonClient::get_object_id(const std::string& path)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::target_path] = path;
    auto rsp(call(GetObjectId::method_name(), req));

    const std::string id(rsp[XMLRPCKeys::object_id]);
    if (id.empty())
    {
        return boost::none;
    }
    else
    {
        return ObjectId(id);
    }
}

std::string
PythonClient::create_snapshot(const std::string& volume_id,
                              const std::string& snapshot_id,
                              const std::string& meta)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = volume_id;
    req[XMLRPCKeys::snapshot_id] = snapshot_id;
    req[XMLRPCKeys::metadata] = XmlRpc::XmlRpcValue(meta.data(), meta.size());

    auto rsp(call(SnapshotCreate::method_name(), req));
    return rsp;
}

std::string
PythonClient::create_volume(const std::string& target_path,
                            boost::shared_ptr<vd::MetaDataBackendConfig> mdb_config,
                            const yt::DimensionedValue& volume_size,
                            const std::string& node_id)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::target_path] = target_path;
    req[XMLRPCKeys::volume_size] = volume_size.toString();

    if (mdb_config)
    {
        req[XMLRPCKeys::metadata_backend_config] =
            XMLRPCStructs::serialize_to_xmlrpc_value(mdb_config->clone());
    }

    if (not node_id.empty())
    {
        req[XMLRPCKeys::vrouter_id] = node_id;
    }

    auto rsp(call(VolumeCreate::method_name(), req));
    return rsp[XMLRPCKeys::volume_id];
}

void
PythonClient::resize(const std::string& object_id,
                     const yt::DimensionedValue& size)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = object_id;
    req[XMLRPCKeys::volume_size] = size.toString();

    call(ResizeObject::method_name(), req);
}

std::string
PythonClient::create_clone(const std::string& target_path,
                           boost::shared_ptr<vd::MetaDataBackendConfig> mdb_config,
                           const std::string& parent_volume_id,
                           const std::string& parent_snap_id,
                           const std::string& node_id)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::target_path] = target_path;
    req[XMLRPCKeys::parent_volume_id] = parent_volume_id;
    req[XMLRPCKeys::parent_snapshot_id] = parent_snap_id;
    if (mdb_config)
    {
        req[XMLRPCKeys::metadata_backend_config] =
            XMLRPCStructs::serialize_to_xmlrpc_value(mdb_config->clone());
    }

    if (not node_id.empty())
    {
        req[XMLRPCKeys::vrouter_id] = node_id;
    }

    auto rsp(call(VolumeClone::method_name(), req));
    return rsp[XMLRPCKeys::volume_id];
}

std::string
PythonClient::create_clone_from_template(const std::string& target_path,
                                         boost::shared_ptr<vd::MetaDataBackendConfig> mdb_config,
                                         const std::string& parent_volume_id,
                                         const std::string& node_id)
{
    return create_clone(target_path,
                        mdb_config,
                        parent_volume_id,
                        "",
                        node_id);
}

void
PythonClient::unlink(const std::string& target_path)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::target_path] = target_path;

    call(Unlink::method_name(), req);
}

void
PythonClient::update_metadata_backend_config(const std::string& volume_id,
                                             boost::shared_ptr<vd::MetaDataBackendConfig>
                                             mdb_config)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = volume_id;
    req[XMLRPCKeys::metadata_backend_config] =
        XMLRPCStructs::serialize_to_xmlrpc_value(mdb_config->clone());

    call(UpdateMetaDataBackendConfig::method_name(), req);
}

uint64_t
PythonClient::volume_potential(const std::string& node_id)
{
    XmlRpc::XmlRpcValue req;
    if(not node_id.empty())
    {
        req[XMLRPCKeys::vrouter_id] = node_id;
    }

    req[XMLRPCKeys::sco_size] = boost::lexical_cast<std::string>(default_sco_size);

    auto rsp(call(VolumePotential::method_name(), req));
    std::string res = rsp[XMLRPCKeys::volume_potential];
    return boost::lexical_cast<uint64_t>(res);
}

void
PythonClient::rollback_volume(const std::string& volume_id,
                              const std::string& snapshot_id)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = volume_id;
    req[XMLRPCKeys::snapshot_id] = snapshot_id;
    call(SnapshotRestore::method_name(), req);
}

void
PythonClient::delete_snapshot(const std::string& volume_id,
                              const std::string& snapshot_id)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = volume_id;
    req[XMLRPCKeys::snapshot_id] = snapshot_id;
    call(SnapshotDestroy::method_name(), req);
}

void
PythonClient::set_volume_as_template(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = volume_id;
    call(SetVolumeAsTemplate::method_name(), req);
}

void
PythonClient::migrate(const std::string& object_id,
                      const std::string& node_id,
                      bool force_restart)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = object_id;
    req[XMLRPCKeys::vrouter_id] = node_id;
    // the python client (used to) use(s) strings throughout.
    XMLRPCUtils::put(req,
                     XMLRPCKeys::force,
                     force_restart);
    call(MigrateObject::method_name(), req);
}

void
PythonClient::stop_object(const std::string& object_id,
                          bool delete_local_data)
{
    XmlRpc::XmlRpcValue req;

    req[XMLRPCKeys::volume_id] = object_id;
    XMLRPCUtils::put(req,
                     XMLRPCKeys::delete_local_data,
                     delete_local_data);

    call(StopObject::method_name(),
         req);
}


void
PythonClient::restart_object(const std::string& object_id,
                             bool force_restart)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = object_id;
    XMLRPCUtils::put(req,
                     XMLRPCKeys::force,
                     force_restart);
    call(RestartObject::method_name(), req);
}

void
PythonClient::set_cluster_cache_behaviour(const std::string& volume_id,
                                          const boost::optional<vd::ClusterCacheBehaviour>& b)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;

    if (b)
    {
        req[XMLRPCKeys::cluster_cache_behaviour] =
            boost::lexical_cast<std::string>(*b);
    }

    call(SetClusterCacheBehaviour::method_name(),
         req);
}

boost::optional<vd::ClusterCacheBehaviour>
PythonClient::get_cluster_cache_behaviour(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;

    auto rsp(call(GetClusterCacheBehaviour::method_name(),
                  req));

    boost::optional<vd::ClusterCacheBehaviour> b;

    if (rsp.hasMember(XMLRPCKeys::cluster_cache_behaviour))
    {
        const std::string m(rsp[XMLRPCKeys::cluster_cache_behaviour]);
        b = boost::lexical_cast<vd::ClusterCacheBehaviour>(m);
    }

    return b;
}

void
PythonClient::set_cluster_cache_mode(const std::string& volume_id,
                                     const boost::optional<vd::ClusterCacheMode>& m)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;

    if (m)
    {
        req[XMLRPCKeys::cluster_cache_mode] =
            boost::lexical_cast<std::string>(*m);
    }

    call(SetClusterCacheMode::method_name(),
         req);
}

boost::optional<vd::ClusterCacheMode>
PythonClient::get_cluster_cache_mode(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;

    auto rsp(call(GetClusterCacheMode::method_name(),
                  req));

    boost::optional<vd::ClusterCacheMode> m;

    if (rsp.hasMember(XMLRPCKeys::cluster_cache_mode))
    {
        const std::string s(rsp[XMLRPCKeys::cluster_cache_mode]);
        m = boost::lexical_cast<vd::ClusterCacheMode>(s);
    }

    return m;
}

std::string
PythonClient::server_revision()
{
    XmlRpc::XmlRpcValue req;
    return call(Revision::method_name(), req);
}

std::string
PythonClient::client_revision()
{
    return yt::buildInfoString();
}

void
PythonClient::mark_node_offline(const std::string& node_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::vrouter_id] = node_id;
    call(MarkNodeOffline::method_name(), req);
}

void
PythonClient::mark_node_online(const std::string& node_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::vrouter_id] = node_id;
    call(MarkNodeOnline::method_name(), req);
}

std::map<NodeId, ClusterNodeStatus::State>
PythonClient::info_cluster()
{
    XmlRpc::XmlRpcValue req;
    auto rsp(call(GetNodesStatusMap::method_name(), req));
    ClusterRegistry::NodeStatusMap nodestatusmap(XMLRPCStructs::deserialize_from_xmlrpc_value<ClusterRegistry::NodeStatusMap>(rsp));

    std::map<NodeId, ClusterNodeStatus::State> ret;
    for (const auto& stat : nodestatusmap)
    {
        ret[stat.first] = stat.second.state;
    }

    return ret;
}

void
PythonClient::update_cluster_node_configs(const std::string& node_id)
{
    XmlRpc::XmlRpcValue req;

    if (not node_id.empty())
    {
        req[XMLRPCKeys::vrouter_id] = node_id;
    }

    call(UpdateClusterNodeConfigs::method_name(), req);
}

void
PythonClient::set_cluster_cache_limit(const std::string& volume_id,
                                      const boost::optional<vd::ClusterCount>& l)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;

    if (l)
    {
        req[XMLRPCKeys::cluster_cache_limit] =
            boost::lexical_cast<std::string>(l->t);
    }

    call(SetClusterCacheLimit::method_name(),
         req);
}

boost::optional<vd::ClusterCount>
PythonClient::get_cluster_cache_limit(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;

    auto rsp(call(GetClusterCacheLimit::method_name(),
                  req));

    boost::optional<vd::ClusterCount> l;

    if (rsp.hasMember(XMLRPCKeys::cluster_cache_limit))
    {
        const std::string s(rsp[XMLRPCKeys::cluster_cache_limit]);
        l = vd::ClusterCount(boost::lexical_cast<uint64_t>(s));
    }

    return l;
}

void
PythonClient::vaai_copy(const std::string& src_path,
                        const std::string& target_path,
                        const uint64_t& timeout,
                        const CloneFileFlags& flags)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::src_path] = src_path;
    req[XMLRPCKeys::target_path] = target_path;
    req[XMLRPCKeys::timeout] = std::to_string(timeout);
    req[XMLRPCKeys::flags] = std::to_string(static_cast<int>(flags));

    call(VAAICopy::method_name(),
         req);
}

LockedPythonClient::Ptr
PythonClient::make_locked_client(const std::string& volume_id,
                                 const unsigned update_interval_secs,
                                 const unsigned grace_period_secs)
{
    LOCK();

    return LockedPythonClient::create(cluster_id_,
                                      cluster_contacts_,
                                      volume_id,
                                      timeout_,
                                      update_interval_secs,
                                      grace_period_secs);
}

vd::TLogName
PythonClient::schedule_backend_sync(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    auto rsp(call(ScheduleBackendSync::method_name(),
                  req));

    const vd::TLogName tlog_name(rsp[XMLRPCKeys::tlog_name]);
    return tlog_name;
}

bool
PythonClient::is_volume_synced_up_to_tlog(const std::string& volume_id,
                                          const vd::TLogName& tlog_name)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    req[XMLRPCKeys::tlog_name] = tlog_name;

    return call(IsVolumeSyncedUpToTLog::method_name(),
                req);
}

bool
PythonClient::is_volume_synced_up_to_snapshot(const std::string& volume_id,
                                              const std::string& snapshot_name)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    req[XMLRPCKeys::snapshot_id] = snapshot_name;

    return call(IsVolumeSyncedUpToSnapshot::method_name(),
                req);
}

boost::optional<size_t>
PythonClient::get_metadata_cache_capacity(const std::string& volume_id)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    auto rsp(call(GetMetaDataCacheCapacity::method_name(), req));

    if (rsp.hasMember(XMLRPCKeys::metadata_cache_capacity))
    {
        return boost::lexical_cast<size_t>(static_cast<std::string>(rsp[XMLRPCKeys::metadata_cache_capacity]));
    }
    else
    {
        return boost::none;
    }
}

void
PythonClient::set_metadata_cache_capacity(const std::string& volume_id,
                                          const boost::optional<size_t>& num_pages)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::volume_id] = volume_id;
    if (num_pages)
    {
        req[XMLRPCKeys::metadata_cache_capacity] =
            boost::lexical_cast<std::string>(*num_pages);
    }
    auto rsp(call(SetMetaDataCacheCapacity::method_name(), req));
}

}
