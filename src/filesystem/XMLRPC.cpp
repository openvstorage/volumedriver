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

#include "ConfigFetcher.h"
#include "FileSystem.h"
#include "ObjectRouter.h"

#include "XMLRPC.h"
#include "XMLRPCKeys.h"
#include "XMLRPCStructs.h"
#include "XMLRPCUtils.h"
#include "CloneFileFlags.h"

#include <cerrno>
#include <fstream>
#include <functional>

#include <boost/filesystem.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/DimensionedValue.h>
#include <youtils/IOException.h>
#include <youtils/System.h>
#include <youtils/wall_timer.h>
#include <youtils/System.h>

#include <volumedriver/Api.h>
#include <volumedriver/MetaDataBackendInterface.h>
#include <volumedriver/ScrubReply.h>
#include <volumedriver/ScrubWork.h>
#include <volumedriver/Types.h>
#include <volumedriver/VolManager.h>
#include <volumedriver/VolumeDriverError.h>
#include <volumedriver/VolumeFailOverState.h>
#include <volumedriver/VolumeOverview.h>
#include <volumedriver/VolumeException.h>

#include <volumedriver/distributed-transaction-log/fungilib/Mutex.h>

namespace volumedriverfs
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

// The following helpers don't need an XMLRPCCall instance - make them static or
//  put them into an anonymous namespace?
void
XMLRPCCallBasic::ensureStruct(::XmlRpc::XmlRpcValue& val)
{
    static std::map<std::string, ::XmlRpc::XmlRpcValue> empty;
    val = ::XmlRpc::XmlRpcValue(empty);
}

vd::VolumeId
XMLRPCCallBasic::getID(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::volume_id);
    return vd::VolumeId(param[XMLRPCKeys::volume_id]);
}

std::string
XMLRPCCallBasic::getScrubbingWorkResult(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::scrubbing_work_result);
    return param[XMLRPCKeys::scrubbing_work_result];
}

std::string
XMLRPCCallBasic::getSnapID(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::snapshot_id);
    return param[XMLRPCKeys::snapshot_id];
}

backend::Namespace
XMLRPCCallBasic::getNS(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::nspace);
    return static_cast<backend::Namespace>(param[XMLRPCKeys::nspace]);
}

uint64_t
XMLRPCCallBasic::getVOLSIZE(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::volume_size);
    return yt::DimensionedValue(std::string(param[XMLRPCKeys::volume_size])).getBytes();
}

uint64_t
XMLRPCCallBasic::getMINSCO(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::min_size);
    return yt::DimensionedValue(std::string(param[XMLRPCKeys::min_size])).getBytes();
}

uint64_t
XMLRPCCallBasic::getMAXSCO(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::max_size);
    return yt::DimensionedValue(std::string(param[XMLRPCKeys::max_size])).getBytes();
}

uint64_t
XMLRPCCallBasic::getMAXNDSCO(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::max_non_disposable_size);
    return yt::DimensionedValue(std::string(param[XMLRPCKeys::max_non_disposable_size])).getBytes();
}

vd::SCOMultiplier
XMLRPCCallBasic::getSCOMULT(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::sco_multiplier);
    return vd::SCOMultiplier(getUIntVal<uint32_t>(param[XMLRPCKeys::sco_multiplier]));
}

vd::LBASize
XMLRPCCallBasic::getLBASIZE(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::lba_size);
    return vd::LBASize(getUIntVal<uint32_t>(param[XMLRPCKeys::lba_size]));
}

vd::ClusterMultiplier
XMLRPCCallBasic::getCLUSTERMULT(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::cluster_multiplier);
    return  vd::ClusterMultiplier(getUIntVal<uint32_t>(param[XMLRPCKeys::cluster_multiplier]));
}

vd::Namespace
XMLRPCCallBasic::getPAR_NS(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::parent_nspace);
    return static_cast<vd::Namespace>(param[XMLRPCKeys::parent_nspace]);
}

std::string
XMLRPCCallBasic::getPAR_SS(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::parent_snapshot_id);
    return static_cast<std::string>(param[XMLRPCKeys::parent_snapshot_id]);
}

vd::PrefetchVolumeData
XMLRPCCallBasic::getPREFETCH(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::prefetch);
    return XMLRPCUtils::get_boolean_enum<vd::PrefetchVolumeData>(param,
                                                  XMLRPCKeys::prefetch);
}

uint64_t
XMLRPCCallBasic::getDelay(::XmlRpc::XmlRpcValue& param)
{
    XMLRPCUtils::ensure_arg(param, XMLRPCKeys::delay);
    return getUIntVal<uint64_t>(param[XMLRPCKeys::delay]);
}

// Y42 Guaranteed Comment Free (this comment will selfdestruct in 5 seconds).
::XmlRpc::XmlRpcValue
XmlRpcArray()
{
    ::XmlRpc::XmlRpcValue arr;
    arr.setSize(0);
    return arr;
}

::XmlRpc::XmlRpcValue
XMLRPCCallBasic::XMLVAL(uint8_t val)
{
    std::stringstream ss;
    ss << static_cast<unsigned>(val);
    return ::XmlRpc::XmlRpcValue(ss.str());
}

::XmlRpc::XmlRpcValue
XMLRPCCallBasic::XMLVAL(int8_t val)
{
    std::stringstream ss;
    ss << static_cast<int>(val);
    return ::XmlRpc::XmlRpcValue(ss.str());
}

::XmlRpc::XmlRpcValue
XMLRPCCallBasic::XMLVAL(bool val)
{
    return ::XmlRpc::XmlRpcValue(bool(val));
}

bool
XMLRPCCallBasic::getbool(::XmlRpc::XmlRpcValue& params, const std::string& name)
{
    XMLRPCUtils::ensure_arg(params, name);
    XmlRpc::XmlRpcValue param = params[name];

    std::string bl(param);
    if (bl == "true" || bl == "True")
    {
        return true;
    }
    else if(bl == "false" || bl == "False")
    {
        return false;
    }
    else
    {
        throw fungi::IOException(("Argument " + bl + " should be [Tt]rue or [Ff]alse").c_str());
    }
}

bool
XMLRPCCallBasic::getboolWithDefault(::XmlRpc::XmlRpcValue& params,
                                    const std::string& name,
                                    const bool def)
{
    XmlRpc::XmlRpcValue param = params[name];

    std::string bl(param);
    if (bl == "true" || bl == "True")
    {
        return true;
    }
    else if(bl == "false" || bl == "False")
    {
        return false;
    }
    else
    {
        return def;
    }
}

void
XMLRPCCallBasic::setError(::XmlRpc::XmlRpcValue& result,
                          const XMLRPCErrorCode error_code,
                          const char* error_string)
{
    result.clear();
    result[XMLRPCKeys::xmlrpc_error_code] = xmlrpc::XmlRpcValue(static_cast<int>(error_code));
    result[XMLRPCKeys::xmlrpc_error_string] = xmlrpc::XmlRpcValue(error_string);
}


#define LOG_XMLRPCINFO(message) LOG_INFO(message)
#define LOG_XMLRPCERROR(message) LOG_ERROR(message)

void
XMLRPCCallBasic::execute(::XmlRpc::XmlRpcValue& params,
                         ::XmlRpc::XmlRpcValue& result)
{
    execute_internal(params,
                     result);
}

template<class T>
void
XMLRPCTimingWrapper<T>::execute(::XmlRpc::XmlRpcValue& params,
                                ::XmlRpc::XmlRpcValue& result)
{
    //TODO [BDV]
    //  - logging on all exceptions
    //  - also log timing information in case of exceptions
    try
    {
        std::stringstream ss;
        params.write(ss);
        LOG_XMLRPCINFO("Arguments for " << T::_name << " are " << ss.str());
        youtils::wall_timer timer;

        XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::vrouter_cluster_id);
        const ClusterId passed_in_cluster_id(params[0][XMLRPCKeys::vrouter_cluster_id]);
        if (passed_in_cluster_id != T::fs_.object_router().cluster_id())
        {
            throw XmlRpc::XmlRpcException(std::string("cluster id mismatch: ") + passed_in_cluster_id);
        }

        T::execute(params, result);
        LOG_XMLRPCINFO("Call " << T::_name << " took " << timer.elapsed() << " seconds");
    }
    catch(ObjectNotRegisteredException& e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        T::setError(result,
                    XMLRPCErrorCode::ObjectNotFound,
                    boost::diagnostic_information(e).c_str());
        return;
    }
    catch(InvalidOperationException& e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        T::setError(result,
                    XMLRPCErrorCode::InvalidOperation,
                    boost::diagnostic_information(e).c_str());
        return;
    }
    catch(CannotSetSelfOfflineException& e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        T::setError(result,
                    XMLRPCErrorCode::InvalidOperation,
                    boost::diagnostic_information(e).c_str());
        return;
    }
    catch(FileExistsException& e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        T::setError(result,
                    XMLRPCErrorCode::FileExists,
                    boost::diagnostic_information(e).c_str());
        return;
    }
    catch(ObjectStillHasChildrenException& e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        T::setError(result,
                    XMLRPCErrorCode::ObjectStillHasChildren,
                    boost::diagnostic_information(e).c_str());
        return;

    }
    catch(vd::VolumeIsTemplateException& e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        T::setError(result,
                    XMLRPCErrorCode::InvalidOperation,
                    e.what());
        return;
    }
    catch(vd::UpdateMetaDataBackendConfigException& e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        T::setError(result,
                    XMLRPCErrorCode::InvalidOperation,
                    e.what());
        return;
    }
    catch(vd::SnapshotNotFoundException& e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        T::setError(result,
                    XMLRPCErrorCode::SnapshotNotFound,
                    e.what());
        return;
    }
    catch(vd::VolManager::InsufficientResourcesException & e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        T::setError(result,
                    XMLRPCErrorCode::InsufficientResources,
                    e.what());
        return;
    }
    catch(vd::PreviousSnapshotNotOnBackendException & e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        T::setError(result,
                    XMLRPCErrorCode::PreviousSnapshotNotOnBackend,
                    e.what());
        return;
    }
    catch(fungi::IOException& e)
    {
        LOG_XMLRPCERROR(T::_name << " Caught fungi::IOException: " << e.what());
        throw ::XmlRpc::XmlRpcException(T::_name + " Caught fungi::IOException: " + e.what(),
                                        1);
    }
    catch(boost::exception& e)
    {
        LOG_XMLRPCERROR(T::_name << " " << boost::diagnostic_information(e));
        throw ::XmlRpc::XmlRpcException(T::_name + " Caught boost::exception: " + boost::diagnostic_information(e),
                                        1);
    }
    catch(std::exception& e)
    {
        LOG_XMLRPCERROR(T::_name << " Caught std::exception: " << e.what());
        throw ::XmlRpc::XmlRpcException(T::_name + " Caught std::exception: " + e.what(),
                                        1);

    }
    catch(XmlRpc::XmlRpcException& e)
    {
        LOG_XMLRPCERROR(T::_name << " Caught XmlRpc::XmlRpcException: " << e.getMessage());
        throw ::XmlRpc::XmlRpcException(T::_name + " Caught XmlRpcException: " + e.getMessage(),
                                        1);
    }
    catch(...)
    {
        LOG_XMLRPCERROR(T::_name << " Caught Unknown exception! This is not good.");
        throw ::XmlRpc::XmlRpcException(T::_name + " Caught unknown exception ",
                                        1);
    }
}

template<class T>
void
XMLRPCLockWrapper<T>::execute(::XmlRpc::XmlRpcValue& params,
                              ::XmlRpc::XmlRpcValue& result)
{
    fungi::ScopedLock l(api::getManagementMutex());
    T::execute(params, result);
}

template<class T>
void
XMLRPCRedirectWrapper<T>::execute(::XmlRpc::XmlRpcValue& params,
                                  ::XmlRpc::XmlRpcValue& result)
{
#ifndef NDEBUG
    std::stringstream ss;
    params.write(ss);
    LOG_INFO("Arguments for " << T::_name << " are " << ss.str());
#endif

    ObjectRouter& vrouter = T::fs_.object_router();
    boost::optional<ClusterNodeConfig> maybe_remote_config;

    if (params[0].hasMember(XMLRPCKeys::vrouter_id))
    {
        const NodeId node_id(params[0][XMLRPCKeys::vrouter_id]);

        LOG_INFO("Execution on " << node_id << " requested");
        if (node_id != vrouter.node_id())
        {
            LOG_INFO("We're not " << node_id << " but " << vrouter.node_id() <<
                     " - redirecting");

            maybe_remote_config = vrouter.node_config(node_id);
            if (not maybe_remote_config)
            {
                LOG_ERROR("Unknown cluster node " << node_id);
                throw fungi::IOException("Unknown cluster node",
                                         node_id.str().c_str());
            }
        }
        else
        {
            LOG_INFO("Node ID " << node_id << " is ours - good");
        }
    }

    // Volumes could move after a lookup, so let's not even bother trying to figure it
    // out beforehand but only deal with the potential fallout of the volume not being
    // here.
    if (not maybe_remote_config)
    {
        try
        {
            T::execute(params, result);
            return;
        }
        catch (ObjectNotRunningHereException&)
        {
            const ObjectId id(T::getID(params[0]).str());

            LOG_INFO("Object " << id <<
                     " is not present on this node - figuring out if it lives elsewhere.");

            const ObjectRegistrationPtr
                reg(vrouter.object_registry()->find_throw(id,
                                                          IgnoreCache::T));
            maybe_remote_config = vrouter.node_config(reg->node_id);
            if (not maybe_remote_config)
            {
                LOG_ERROR("Object " << id <<
                          " does not exist locally and there is no remote node configured");
                throw;
            }
        }
    }

    VERIFY(maybe_remote_config);

    LOG_INFO("Redirecting to " << maybe_remote_config->host << ", port " <<
             maybe_remote_config->xmlrpc_port);

    result.clear();
    result[XMLRPCKeys::xmlrpc_redirect_host] = T::XMLVAL(maybe_remote_config->host);
    result[XMLRPCKeys::xmlrpc_redirect_port] = T::XMLVAL(maybe_remote_config->xmlrpc_port);
}

//Not using an XMLRPC_WRAPPER for exception conversion
//   from volumedriver::VolumeDoesNotExistException
//   to   volumedriver_fs::ObjectNotRunningHereException
//as this is rather tightly bound to the actual meat of the XMLRPC call:

namespace
{

void with_api_exception_conversion(std::function<void()>&& fn)
{
    try
    {
        fn();
    }
    catch (vd::VolManager::VolumeDoesNotExistException& e)
    {
        throw ObjectNotRunningHereException(e.what());
    }
}

}

void
VolumesList::execute_internal(::XmlRpc::XmlRpcValue& params,
                              ::XmlRpc::XmlRpcValue& result)
{
    result.clear();
    result.setSize(0);

    if (params[0].hasMember(XMLRPCKeys::vrouter_id))
    {
        fungi::ScopedLock g(api::getManagementMutex());
        std::list<vd::VolumeId> l;
        api::getVolumeList(l);

        int k = 0;
        for (const auto& v : l)
        {
            result[k++] = ::XmlRpc::XmlRpcValue(v.str());
        }
    }
    else
    {
        auto registry(fs_.object_router().object_registry());
        const auto objs(registry->list());

        int k = 0;

        for(const auto& o : objs)
        {
            const auto reg(registry->find(o,
                                          IgnoreCache::F));
            if (reg->object().type == ObjectType::Volume or
                reg->object().type == ObjectType::Template)
            {
                result[k++] = ::XmlRpc::XmlRpcValue(o);
            }
        }
    }
}

void
VolumesListByPath::execute_internal(::XmlRpc::XmlRpcValue& /* params */,
                                    ::XmlRpc::XmlRpcValue& result)
{
    auto registry(fs_.object_router().object_registry());
    const auto objs(registry->list());

    result.clear();
    result.setSize(0);

    int k = 0;

    for (const auto& o: objs)
    {
        const auto reg(registry->find(o,
                                      IgnoreCache::F));
        if (reg->object().type == ObjectType::Volume or
            reg->object().type == ObjectType::Template)
        {
            const FrontendPath volume_path(fs_.find_path(reg->volume_id));
            result[k++] = ::XmlRpc::XmlRpcValue(volume_path.string());
        }
    }
}

void
VolumePotential::execute_internal(::XmlRpc::XmlRpcValue& params,
                                  ::XmlRpc::XmlRpcValue& result)
{
    with_api_exception_conversion([&]
    {
        auto param = params[0];

        boost::optional<vd::ClusterSize> cluster_size;

        if (param.hasMember(XMLRPCKeys::cluster_size))
        {
            const std::string s(param[XMLRPCKeys::cluster_size]);
            cluster_size = boost::lexical_cast<vd::ClusterSize>(s);
        }

        boost::optional<vd::SCOMultiplier> sco_mult;

        if (param.hasMember(XMLRPCKeys::sco_multiplier))
        {
            const std::string s(param[XMLRPCKeys::sco_multiplier]);
            sco_mult = boost::lexical_cast<vd::SCOMultiplier>(s);
        }

        boost::optional<vd::TLogMultiplier> tlog_mult;

        if (param.hasMember(XMLRPCKeys::tlog_multiplier))
        {
            const std::string s(param[XMLRPCKeys::tlog_multiplier]);
            const uint32_t n = boost::lexical_cast<uint32_t>(s);
            tlog_mult = vd::TLogMultiplier(n);
        }

        const uint64_t pot = fs_.object_router().local_volume_potential(cluster_size,
                                                                        sco_mult,
                                                                        tlog_mult);
        result[XMLRPCKeys::volume_potential] = XMLVAL(pot);
    });
}

void
GetSyncIgnore::execute_internal(::XmlRpc::XmlRpcValue& params,
                                ::XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId vol_id(getID(params[0]));
    with_api_exception_conversion
        ([&]()
         {
             uint64_t  number_of_syncs_to_ignore,
                 maximum_time_to_ignore_syncs_in_seconds;
             api::getSyncIgnore(vol_id,
                                number_of_syncs_to_ignore,
                                maximum_time_to_ignore_syncs_in_seconds);
             ensureStruct(result);
             result[XMLRPCKeys::number_of_syncs_to_ignore] = XMLVAL(number_of_syncs_to_ignore);
             result[XMLRPCKeys::maximum_time_to_ignore_syncs_in_seconds] = XMLVAL(maximum_time_to_ignore_syncs_in_seconds);
         });
}

void
SetSyncIgnore::execute_internal(::XmlRpc::XmlRpcValue& params,
                                ::XmlRpc::XmlRpcValue& /* result */)
{
    const vd::VolumeId vol_id(getID(params[0]));
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::number_of_syncs_to_ignore);
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::maximum_time_to_ignore_syncs_in_seconds);
    with_api_exception_conversion([&]()
    {
        api::setSyncIgnore(vol_id,
                           getUIntVal<uint64_t>(params[0][XMLRPCKeys::number_of_syncs_to_ignore]),
                           getUIntVal<uint64_t>(params[0][XMLRPCKeys::maximum_time_to_ignore_syncs_in_seconds]));
    });
}

void
GetSCOMultiplier::execute_internal(::XmlRpc::XmlRpcValue& params,
                                   ::XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId vol_id(getID(params[0]));
    with_api_exception_conversion
        ([&]()
         {
             uint32_t  sco_multiplier = api::getSCOMultiplier(vol_id);
             ensureStruct(result);
             result[XMLRPCKeys::sco_multiplier] = XMLVAL(sco_multiplier);
         });
}

void
SetSCOMultiplier::execute_internal(::XmlRpc::XmlRpcValue& params,
                                   ::XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId vol_id(getID(params[0]));
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::sco_multiplier);
    try
    {
        with_api_exception_conversion([&]()
        {
            const std::string s(params[0][XMLRPCKeys::sco_multiplier]);
            const vd::SCOMultiplier m(boost::lexical_cast<uint32_t>(s));
            api::setSCOMultiplier(vol_id,
                                  m);
        });
    }
    catch (vd::InvalidOperation& e)
    {
        setError(result, XMLRPCErrorCode::InvalidOperation, e.what());
    }
}

void
GetTLogMultiplier::execute_internal(::XmlRpc::XmlRpcValue& params,
                                    ::XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId vol_id(getID(params[0]));
    with_api_exception_conversion
        ([&]()
         {
             ensureStruct(result);
             boost::optional<volumedriver::TLogMultiplier> tlog_multiplier = api::getTLogMultiplier(vol_id);
             if (tlog_multiplier)
             {
                 result[XMLRPCKeys::tlog_multiplier] = XMLVAL(tlog_multiplier.get());
             }
         });
}

void
SetTLogMultiplier::execute_internal(::XmlRpc::XmlRpcValue& params,
                                    ::XmlRpc::XmlRpcValue& result)
{
    try
    {
        with_api_exception_conversion([&]()
        {
            auto param = params[0];
            const vd::VolumeId vol_id(getID(param));
            boost::optional<vd::TLogMultiplier> tlog_multiplier;
            if (param.hasMember(XMLRPCKeys::tlog_multiplier))
            {
                const std::string s(param[XMLRPCKeys::tlog_multiplier]);
                const uint32_t n = boost::lexical_cast<uint32_t>(s);
                tlog_multiplier = vd::TLogMultiplier(n);
            }
            api::setTLogMultiplier(vol_id,
                                   tlog_multiplier);
        });
    }
    catch (vd::InvalidOperation& e)
    {
        setError(result, XMLRPCErrorCode::InvalidOperation, e.what());
        return;
    }
    // OK
    result.clear();
}

void
GetSCOCacheMaxNonDisposableFactor::execute_internal(::XmlRpc::XmlRpcValue& params,
                                                    ::XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId vol_id(getID(params[0]));
    with_api_exception_conversion
        ([&]()
         {
             ensureStruct(result);
             boost::optional<volumedriver::SCOCacheNonDisposableFactor> max_non_disposable_factor = api::getSCOCacheMaxNonDisposableFactor(vol_id);
             if (max_non_disposable_factor)
             {
                 result[XMLRPCKeys::max_non_disposable_factor] = XMLVAL(max_non_disposable_factor.get());
             }
         });
}

void
SetSCOCacheMaxNonDisposableFactor::execute_internal(::XmlRpc::XmlRpcValue& params,
                                                    ::XmlRpc::XmlRpcValue& result)
{
    try
    {
        with_api_exception_conversion([&]()
        {
            auto param = params[0];
            const vd::VolumeId vol_id(getID(param));
            boost::optional<vd::SCOCacheNonDisposableFactor> max_non_disposable_factor;
            if (param.hasMember(XMLRPCKeys::max_non_disposable_factor))
            {
                std::string s(param[XMLRPCKeys::max_non_disposable_factor]);
                max_non_disposable_factor = boost::optional<vd::SCOCacheNonDisposableFactor>(boost::lexical_cast<float>(s));
            }
            api::setSCOCacheMaxNonDisposableFactor(vol_id,
                                                   max_non_disposable_factor);
        });
    }
    catch (vd::InvalidOperation& e)
    {
        setError(result, XMLRPCErrorCode::InvalidOperation, e.what());
        return;
    }
    // OK
    result.clear();
}

void
GetScrubbingWork::execute_internal(::XmlRpc::XmlRpcValue& params,
                                   ::XmlRpc::XmlRpcValue& result)
{
    const ObjectId volid(getID(params[0]));
    std::string begin_snapshot;
    std::string end_snapshot;
    boost::optional<vd::SnapshotName> ssnap;
    if (params[0].hasMember(XMLRPCKeys::start_snapshot))
    {
        ssnap = vd::SnapshotName(params[0][XMLRPCKeys::start_snapshot]);
    }

    boost::optional<vd::SnapshotName> esnap;
    if (params[0].hasMember(XMLRPCKeys::end_snapshot))
    {
        esnap = vd::SnapshotName(params[0][XMLRPCKeys::end_snapshot]);
    }

    const std::vector<scrubbing::ScrubWork>
        work(fs_.object_router().get_scrub_work(volid,
                                                ssnap,
                                                esnap));

    result.clear();
    result.setSize(0);
    int k = 0;
    for(const auto& w : work)
    {
        result[k++] = ::XmlRpc::XmlRpcValue(w.str());
    }
}

void
ApplyScrubbingResult::execute_internal(::XmlRpc::XmlRpcValue&  params,
                                       ::XmlRpc::XmlRpcValue& /*result*/)
{
    const ObjectId volid(getID(params[0]));
    const std::string scrub_rsp_str(getScrubbingWorkResult(params[0]));

    fs_.object_router().queue_scrub_reply(volid,
                                          scrubbing::ScrubReply(scrub_rsp_str));
}

void
VolumeInfo::execute_internal(::XmlRpc::XmlRpcValue& params,
                             ::XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId vol_id(getID(params[0]));

    XMLRPCVolumeInfo volume_info;

    with_api_exception_conversion([&]()
    {
        fungi::ScopedLock l(api::getManagementMutex());

        const vd::VolumeConfig& cfg = api::getVolumeConfig(vol_id);
        const boost::optional<vd::FailOverCacheConfig>&
            maybe_focconfig = api::getFailOverCacheConfig(vol_id);
        vd::MetaDataStoreStats stats = api::getMetaDataStoreStats(vol_id);

        volume_info.volume_id = cfg.id_;
        volume_info._namespace_ = cfg.getNS().str();
        volume_info.parent_namespace = cfg.parent() ?
            cfg.parent()->nspace.str() : std::string("");
        volume_info.parent_snapshot_id = cfg.parent() ?
            cfg.parent()->snapshot : std::string("");
        volume_info.volume_size = cfg.lba_size_ * cfg.lba_count();
        volume_info.lba_size = cfg.lba_size_;
        volume_info.lba_count = cfg.lba_count();
        volume_info.cluster_multiplier = cfg.cluster_mult_;
        volume_info.sco_multiplier = cfg.sco_mult_;
        volume_info.tlog_multiplier = cfg.tlog_mult_;
        volume_info.max_non_disposable_factor = cfg.max_non_disposable_factor_;
        volume_info.metadata_backend_config = cfg.metadata_backend_config_->clone();

        volume_info.failover_mode = vd::volumeFailoverStateToString(api::getFailOverMode(vol_id));
        volume_info.failover_ip = maybe_focconfig ?
            maybe_focconfig->host : std::string();
        volume_info.failover_port = maybe_focconfig ?
            maybe_focconfig->port : 0;
        volume_info.halted = api::getHalted(vol_id);
        volume_info.footprint = stats.used_clusters * cfg.cluster_mult_ * cfg.lba_size_;
        volume_info.stored = api::getStored(vol_id);

        vd::SharedVolumePtr v(api::getVolumePointer(vol_id));
        volume_info.owner_tag =
            static_cast<uint64_t>(v->getOwnerTag());
        volume_info.cluster_cache_handle =
            static_cast<uint64_t>(v->getClusterCacheHandle());
        volume_info.cluster_cache_limit = v->get_cluster_cache_limit();
    });

    const ObjectId oid(vol_id.str());
    ObjectRegistrationPtr reg(fs_.object_router().object_registry()->find_throw(oid,
                                                                                IgnoreCache::F));

    volume_info.object_type = reg->treeconfig.object_type;
    volume_info.parent_volume_id = reg->treeconfig.parent_volume ?
        reg->treeconfig.parent_volume->str() :
        std::string("");
    volume_info.vrouter_id = reg->node_id;

    result = XMLRPCStructs::serialize_to_xmlrpc_value(volume_info);
}

void
SnapshotCreate::execute_internal(::XmlRpc::XmlRpcValue& params,
                                 ::XmlRpc::XmlRpcValue& result)
{
    with_api_exception_conversion([&]()
    {
        const vd::VolumeId volName(getID(params[0]));
        const vd::SnapshotName snap_id(getSnapID(params[0]));

        vd::SnapshotMetaData metadata;

        if (params[0].hasMember(XMLRPCKeys::metadata))
        {
            metadata = params[0][XMLRPCKeys::metadata];
        }

        const std::string snapname(api::createSnapshot(volName,
                                                       metadata,
                                                       &snap_id));
        result = XMLVAL(snapname);
    });
}

void
UpdateMetaDataBackendConfig::execute_internal(::XmlRpc::XmlRpcValue& params,
                                              ::XmlRpc::XmlRpcValue& /*result*/)
{
    with_api_exception_conversion([&]()
    {
        const vd::VolumeId volume_id(getID(params[0]));
        vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config;
        mdb_config = XMLRPCStructs::deserialize_from_xmlrpc_value<decltype(mdb_config)>(params[0][XMLRPCKeys::metadata_backend_config]);

        api::updateMetaDataBackendConfig(volume_id,
                                         *mdb_config);
    });
}

void
SnapshotsList::execute_internal(::XmlRpc::XmlRpcValue& params,
                                ::XmlRpc::XmlRpcValue& result)
{
    with_api_exception_conversion([&]()
    {
        std::list<vd::SnapshotName> l;
        const vd::VolumeId volName(getID(params[0]));
        api::showSnapshots(volName, l);

        result.clear();
        result.setSize(0);
        size_t k = 0;

        for(std::list<vd::SnapshotName>::const_iterator i = l.begin();  i != l.end(); ++i)
        {
            result[k++] = *i;
        }
    });
}

void
SnapshotInfo::execute_internal(::XmlRpc::XmlRpcValue& params,
                               ::XmlRpc::XmlRpcValue& result)
{
    with_api_exception_conversion([&]()
    {
        std::list<std::string> l;
        const vd::VolumeId vol_id(getID(params[0]));
        const vd::SnapshotName snap_id(getSnapID(params[0]));

        const auto snap(api::getSnapshot(vol_id, snap_id));
        const auto& meta(snap.metadata());
        const XMLRPCSnapshotInfo snap_info(snap.getName(),
                                           snap.getDate(),
                                           snap.getUUID().str(),
                                           snap.backend_size(),
                                           snap.inBackend(),
                                           std::string(meta.begin(), meta.end()));

        result = XMLRPCStructs::serialize_to_xmlrpc_value(snap_info);
    });
}

void
VolumeCreate::execute_internal(::XmlRpc::XmlRpcValue& params,
                               ::XmlRpc::XmlRpcValue& result)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::target_path);

    const FrontendPath path(static_cast<const std::string&>(params[0][XMLRPCKeys::target_path]));
    const uint64_t vsize(getVOLSIZE(params[0]));

    vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config;

    if (params[0].hasMember(XMLRPCKeys::metadata_backend_config))
    {
        mdb_config =
            XMLRPCStructs::deserialize_from_xmlrpc_value<decltype(mdb_config)>(params[0][XMLRPCKeys::metadata_backend_config]);
    }

    const ObjectId id(fs_.create_volume(path,
                                        std::move(mdb_config),
                                        vsize));

    result[XMLRPCKeys::volume_id] = ::XmlRpc::XmlRpcValue(id);
}

void
Unlink::execute_internal(::XmlRpc::XmlRpcValue& params,
                         ::XmlRpc::XmlRpcValue& /*result*/)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::target_path);
    const FrontendPath path(static_cast<const std::string&>(params[0][XMLRPCKeys::target_path]));
    fs_.unlink(path);
}

void
VolumeClone::execute_internal(::XmlRpc::XmlRpcValue& params,
                              ::XmlRpc::XmlRpcValue& result)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::target_path);
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::parent_volume_id);
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::parent_snapshot_id);

    const FrontendPath path(static_cast<const std::string&>(params[0][XMLRPCKeys::target_path]));
    const vd::VolumeId parent(params[0][XMLRPCKeys::parent_volume_id]);
    const vd::SnapshotName snap(params[0][XMLRPCKeys::parent_snapshot_id]);

    vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config;

    if (params[0].hasMember(XMLRPCKeys::metadata_backend_config))
    {
        mdb_config =
            XMLRPCStructs::deserialize_from_xmlrpc_value<decltype(mdb_config)>(params[0][XMLRPCKeys::metadata_backend_config]);
    }

    const ObjectId id(fs_.create_clone(path,
                                       std::move(mdb_config),
                                       parent,
                                       snap.empty() ?
                                       MaybeSnapshotName() :
                                       MaybeSnapshotName(snap)));

    result[XMLRPCKeys::volume_id] = ::XmlRpc::XmlRpcValue(id);
}

void
SnapshotRestore::execute_internal(::XmlRpc::XmlRpcValue& params,
                                  ::XmlRpc::XmlRpcValue& /*result*/)
{
    const ObjectId volName(getID(params[0]));
    const vd::SnapshotName snapid(getSnapID(params[0]));
    fs_.object_router().rollback_volume(volName, snapid);
}

void
SnapshotDestroy::execute_internal(::XmlRpc::XmlRpcValue& params,
                                  ::XmlRpc::XmlRpcValue& /*result*/)
{
    const ObjectId volName(getID(params[0]));
    const vd::SnapshotName snapid(getSnapID(params[0]));

    fs_.object_router().delete_snapshot(volName, snapid);
}

void
VolumePerformanceCounters::execute_internal(XmlRpc::XmlRpcValue& params,
                                            XmlRpc::XmlRpcValue& result)
{
    with_api_exception_conversion([&]()
    {
        const vd::VolumeId volName(getID(params[0]));
        const vd::MetaDataStoreStats mdStats(api::getMetaDataStoreStats(volName));

        XMLRPCStatistics results_stats;

        results_stats.sco_cache_hits = api::getCacheHits(volName);
        results_stats.sco_cache_misses = api::getCacheMisses(volName);
        results_stats.cluster_cache_hits = api::getClusterCacheHits(volName);
        results_stats.cluster_cache_misses = api::getClusterCacheMisses(volName);
        results_stats.metadata_store_hits = mdStats.cache_hits;
        results_stats.metadata_store_misses = mdStats.cache_misses;
        results_stats.stored = api::getStored(volName);

        vd::PerformanceCounters& perf_counters = api::performance_counters(volName);
        results_stats.performance_counters = perf_counters;

        if (params[0].hasMember(XMLRPCKeys::reset))
        {
            const bool reset = getboolWithDefault(params[0],
                                                  XMLRPCKeys::reset,
                                                  false);
            if (reset)
            {
                perf_counters.reset_all_counters();
            }
        }

        result = XMLRPCStructs::serialize_to_xmlrpc_value(results_stats);
    });
}

void
VolumeDriverPerformanceCounters::execute_internal(XmlRpc::XmlRpcValue& params,
                                                  XmlRpc::XmlRpcValue& result)
{
    bool reset = false;

    if (params[0].hasMember(XMLRPCKeys::reset))
    {
        reset = getboolWithDefault(params[0],
                                   XMLRPCKeys::reset,
                                   false);
    }

    XMLRPCStatistics stats;
    vd::PerformanceCounters perf_counters;

    std::list<vd::VolumeId> l;
    api::getVolumeList(l);


    for (const auto& v : l)
    {
        const vd::MetaDataStoreStats mds(api::getMetaDataStoreStats(v));
        stats.sco_cache_hits += api::getCacheHits(v);
        stats.sco_cache_misses += api::getCacheMisses(v);
        stats.cluster_cache_hits += api::getClusterCacheHits(v);
        stats.cluster_cache_misses += api::getClusterCacheMisses(v);
        stats.metadata_store_hits += mds.cache_hits;
        stats.metadata_store_misses += mds.cache_misses;
        stats.stored += api::getStored(v);

        vd::PerformanceCounters& pc = api::performance_counters(v);

        perf_counters += pc;

        if (reset)
        {
            pc.reset_all_counters();
        }
    }

    stats.performance_counters = perf_counters;

    result = XMLRPCStructs::serialize_to_xmlrpc_value(stats);
}

void
SetVolumeAsTemplate::execute_internal(XmlRpc::XmlRpcValue& params,
                                      XmlRpc::XmlRpcValue& /*result*/)
{
    const vd::VolumeId volName(getID(params[0]));
    fs_.object_router().set_volume_as_template(volName);
}

void
Revision::execute_internal(XmlRpc::XmlRpcValue& /*params*/,
                           XmlRpc::XmlRpcValue& result)
{
    result = api::getRevision();
}

void
MarkNodeOffline::execute_internal(XmlRpc::XmlRpcValue& params,
                           XmlRpc::XmlRpcValue& /*result*/)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::vrouter_id);
    const NodeId node_id(params[0][XMLRPCKeys::vrouter_id]);
    fs_.object_router().mark_node_offline(node_id);
}

void
MarkNodeOnline::execute_internal(XmlRpc::XmlRpcValue& params,
                                 XmlRpc::XmlRpcValue& /*result*/)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::vrouter_id);
    const NodeId node_id(params[0][XMLRPCKeys::vrouter_id]);
    fs_.object_router().mark_node_online(node_id);
}

void
GetNodesStatusMap::execute_internal(XmlRpc::XmlRpcValue& /*params*/,
                            XmlRpc::XmlRpcValue& result)
{
    result = XMLRPCStructs::serialize_to_xmlrpc_value(fs_.object_router().get_node_status_map());
}

void
IsVolumeSyncedUpToSnapshot::execute_internal(XmlRpc::XmlRpcValue& params,
                                             XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const vd::SnapshotName snapshotName(getSnapID(params[0]));

    with_api_exception_conversion([&]()
                                  {
                                      result = XMLVAL(api::isVolumeSyncedUpTo(volName,
                                                                              snapshotName));
                                  });
}

void
DataStoreWriteUsed::execute_internal(XmlRpc::XmlRpcValue& params,
                                     XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const uint64_t res = api::VolumeDataStoreWriteUsed(volName);
    result[XMLRPCKeys::data_store_write_used] = XMLVAL(res);
}

void
DataStoreReadUsed::execute_internal(XmlRpc::XmlRpcValue& params,
                                    XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const uint64_t res = api::VolumeDataStoreReadUsed(volName);
    result[XMLRPCKeys::data_store_read_used] = XMLVAL(res);
}


void
QueueCount::execute_internal(XmlRpc::XmlRpcValue& params,
                             XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const uint64_t res = api::getQueueCount(volName);
    result[XMLRPCKeys::queue_count] = XMLVAL(res);
}

void
QueueSize::execute_internal(XmlRpc::XmlRpcValue& params,
                            XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const uint64_t res = api::getQueueSize(volName);
    result[XMLRPCKeys::queue_size] = XMLVAL(res);
}

void
TLogUsed::execute_internal(XmlRpc::XmlRpcValue& params,
                           XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const uint64_t res = api::getTLogUsed(volName);
    result[XMLRPCKeys::tlog_used] = XMLVAL(res);
}

void
ScheduleBackendSync::execute_internal(XmlRpc::XmlRpcValue& params,
                                      XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    with_api_exception_conversion([&]
                                  {
                                      const vd::TLogId
                                          tlog_id(api::scheduleBackendSync(volName));
                                      result[XMLRPCKeys::tlog_name] =
                                          XMLVAL(boost::lexical_cast<std::string>(tlog_id));
                                  });
}

void
IsVolumeSyncedUpToTLog::execute_internal(XmlRpc::XmlRpcValue& params,
                                         XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    XMLRPCUtils::ensure_arg(params[0],
                            XMLRPCKeys::tlog_name);
    const vd::TLogName tlog_name(params[0][XMLRPCKeys::tlog_name]);
    const auto tlog_id(boost::lexical_cast<vd::TLogId>(tlog_name));

    with_api_exception_conversion([&]
                                  {
                                      result = XMLVAL(api::isVolumeSyncedUpTo(volName,
                                                                              tlog_id));
                                  });
}

void
RemoveNamespaceFromSCOCache::execute_internal(XmlRpc::XmlRpcValue& params,
                                              XmlRpc::XmlRpcValue& result)
{

    api::removeNamespaceFromSCOCache(getNS(params[0]));
    result = XmlRpc::XmlRpcValue(true);
}

void
ScoCacheInfo::execute_internal(XmlRpc::XmlRpcValue& /*params*/,
                               XmlRpc::XmlRpcValue& result)
{
    result.clear();

    vd::SCOCacheMountPointsInfo info;
    api::getSCOCacheMountPointsInfo(info);
    ensureStruct(result);

    for (const auto& i : info)
    {
        XmlRpc::XmlRpcValue val;
        val[XMLRPCKeys::max_size] = XMLVAL(yt::DimensionedValue(i.second.capacity).toString());
        val[XMLRPCKeys::free] = XMLVAL(i.second.free);
        val[XMLRPCKeys::used] = XMLVAL(i.second.used);
        val[XMLRPCKeys::choking] = XMLVAL(i.second.choking);
        val[XMLRPCKeys::offlined] = XMLVAL(i.second.offlined);
        result[i.first.string()] = val;
    }
}

void
VolumeScoCacheInfo::execute_internal(XmlRpc::XmlRpcValue& params,
                                     XmlRpc::XmlRpcValue& result)
{
    result.clear();
    const backend::Namespace ns(getID(params[0]));

    const vd::SCOCacheNamespaceInfo info(api::getVolumeSCOCacheInfo(ns));

    result[XMLRPCKeys::max_non_disposable_size] =
        XMLVAL(yt::DimensionedValue(info.max_non_disposable).toString());
    result[XMLRPCKeys::min_size] = XMLVAL(yt::DimensionedValue(info.min).toString());
    result[XMLRPCKeys::disposable] = XMLVAL(info.disposable);
    result[XMLRPCKeys::non_disposable] = XMLVAL(info.nondisposable);
}

void
GetFailOverMode::execute_internal(XmlRpc::XmlRpcValue& params,
                                  XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    result = vd::volumeFailoverStateToString(api::getFailOverMode(volName));
}

void
CurrentSCOCount::execute_internal(XmlRpc::XmlRpcValue& params,
                                  XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const uint64_t res = api::getCurrentSCOCount(volName);
    result[XMLRPCKeys::current_sco_count] = XMLVAL(res);
}

void
SnapshotSCOCount::execute_internal(XmlRpc::XmlRpcValue& params,
                                   XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const vd::SnapshotName snapName(getSnapID(params[0]));
    uint64_t res  = api::getSnapshotSCOCount(volName,
                                             snapName);
    result[XMLRPCKeys::snapshot_sco_count] = XMLVAL(res);
}

void
CheckVolumeConsistency::execute_internal(XmlRpc::XmlRpcValue& params,
                                         XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const bool res = api::checkVolumeConsistency(volName);
    result = XMLVAL(res);
}

void
CurrentBackendSize::execute_internal(XmlRpc::XmlRpcValue& params,
                                     XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const uint64_t res = api::getCurrentBackendSize(volName);
    result = XMLVAL(res);
}

void
SnapshotBackendSize::execute_internal(XmlRpc::XmlRpcValue& params,
                                      XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId volName(getID(params[0]));
    const vd::SnapshotName snapName(getSnapID(params[0]));
    uint64_t res = api::getSnapshotBackendSize(volName,
                                               snapName);
    result = XMLVAL(res);
}

void
DumpSCOInfo::execute_internal(XmlRpc::XmlRpcValue& params,
                              XmlRpc::XmlRpcValue& /* result */)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::file_name);
    const std::string fname(params[0][XMLRPCKeys::file_name]);
    api::dumpSCOInfo(fname);
}

void
MallocInfo::execute_internal(XmlRpc::XmlRpcValue& /* params */,
                             XmlRpc::XmlRpcValue& result)
{
    result = yt::System::malloc_info();
}

void
StartPrefetching::execute_internal(XmlRpc::XmlRpcValue& params,
                                   XmlRpc::XmlRpcValue& /*result*/)
{
    const vd::VolumeId volName(getID(params[0]));
    api::startPrefetching(volName);
}

void
PersistConfigurationToString::execute_internal(XmlRpc::XmlRpcValue& params,
                                               XmlRpc::XmlRpcValue& result)
{

    const bool reportDefault = getboolWithDefault(params[0],
                                                  XMLRPCKeys::show_defaults,
                                                  false);

    const std::string res = api::persistConfiguration(reportDefault);
    result[XMLRPCKeys::configuration] = XMLVAL(res);
}

void
CheckConfiguration::execute_internal(XmlRpc::XmlRpcValue& params,
                                     XmlRpc::XmlRpcValue& result)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::configuration_path);
    const std::string configuration_path(params[0][XMLRPCKeys::configuration_path]);
    vd::ConfigurationReport c_rep;
    const bool success = api::checkConfiguration(configuration_path,
                                                 c_rep);
    result[XMLRPCKeys::success] = XMLVAL(success);
    int i = 0;
    XmlRpc::XmlRpcValue problems = XmlRpcArray();

    for (const auto& prob : c_rep)
    {
        XmlRpc::XmlRpcValue val;
        val[XMLRPCKeys::param_name] = XMLVAL(prob.param_name());
        val[XMLRPCKeys::component_name] = XMLVAL(prob.component_name());
        val[XMLRPCKeys::problem] = XMLVAL(prob.problem());
        problems[i++] = val;
    }
    result[XMLRPCKeys::problems] = problems;
}

void
UpdateConfiguration::execute_internal(XmlRpc::XmlRpcValue& params,
                                      XmlRpc::XmlRpcValue& result)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::configuration_path);
    const std::string config(params[0][XMLRPCKeys::configuration_path]);

    const boost::variant<yt::UpdateReport,
                         yt::ConfigurationReport>
        rep(api::updateConfiguration(ConfigFetcher(config)(VerifyConfig::T)));

    result = XMLRPCStructs::serialize_to_xmlrpc_value(rep);
}

void
GetVolumeDriverState::execute_internal(XmlRpc::XmlRpcValue& /*params*/,
                                       XmlRpc::XmlRpcValue& result)
{
    const bool isReadOnly = api::getVolumeDriverReadOnly();
    result[XMLRPCKeys::volumedriver_readonly] = XMLVAL(isReadOnly);
}

void
SetClusterCacheBehaviour::execute_internal(XmlRpc::XmlRpcValue& params,
                                           XmlRpc::XmlRpcValue& /* result */)
{
    with_api_exception_conversion([&]()
    {
        const std::string volname(getID(params[0]));
        boost::optional<vd::ClusterCacheBehaviour> behaviour;
        if(params[0].hasMember(XMLRPCKeys::cluster_cache_behaviour))
        {
            std::stringstream ss(params[0][XMLRPCKeys::cluster_cache_behaviour]);
            ss.exceptions(fs::ofstream::failbit bitor fs::ofstream::badbit);
            vd::ClusterCacheBehaviour b = vd::ClusterCacheBehaviour::NoCache;
            ss >> b;
            behaviour = b;
        }
        api::setClusterCacheBehaviour(vd::VolumeId(volname),
                                      behaviour);
    });
}

void
GetClusterCacheBehaviour::execute_internal(XmlRpc::XmlRpcValue& params,
                                           XmlRpc::XmlRpcValue&  result)
{
    with_api_exception_conversion([&]()
    {
        const std::string volname(getID(params[0]));
        boost::optional<vd::ClusterCacheBehaviour>
            behaviour(api::getClusterCacheBehaviour(vd::VolumeId(volname)));
        if(behaviour)
        {
            std::stringstream ss;
            ss << *behaviour;
            result[XMLRPCKeys::cluster_cache_behaviour] = XMLVAL(ss.str());
        }
    });
}

void
SetClusterCacheMode::execute_internal(XmlRpc::XmlRpcValue& params,
                                      XmlRpc::XmlRpcValue& /* result */)
{
    with_api_exception_conversion([&]()
    {
        const std::string volname(getID(params[0]));
        boost::optional<vd::ClusterCacheMode> mode;
        if(params[0].hasMember(XMLRPCKeys::cluster_cache_mode))
        {
            std::stringstream ss(params[0][XMLRPCKeys::cluster_cache_mode]);
            ss.exceptions(fs::ofstream::failbit bitor fs::ofstream::badbit);
            vd::ClusterCacheMode m = vd::ClusterCacheMode::ContentBased;
            ss >> m;
            mode = m;
        }
        api::setClusterCacheMode(vd::VolumeId(volname),
                                 mode);
    });
}

void
GetClusterCacheMode::execute_internal(XmlRpc::XmlRpcValue& params,
                                      XmlRpc::XmlRpcValue&  result)
{
    with_api_exception_conversion([&]()
    {
        const std::string volname(getID(params[0]));
        boost::optional<vd::ClusterCacheMode>
            mode(api::getClusterCacheMode(vd::VolumeId(volname)));

        if(mode)
        {
            std::stringstream ss;
            ss << *mode;
            result[XMLRPCKeys::cluster_cache_mode] = XMLVAL(ss.str());
        }
    });
}

void
SetClusterCacheLimit::execute_internal(XmlRpc::XmlRpcValue& params,
                                       XmlRpc::XmlRpcValue& /* result */)
{
    with_api_exception_conversion([&]()
    {
        auto& param = params[0];

        const std::string volname(getID(param));
        boost::optional<vd::ClusterCount> limit;
        if(param.hasMember(XMLRPCKeys::cluster_cache_limit))
        {
            auto& val = param[XMLRPCKeys::cluster_cache_limit];
            limit = vd::ClusterCount(getUIntVal<uint64_t>(val));
        }

        api::setClusterCacheLimit(vd::VolumeId(volname),
                                  limit);
    });
}

void
GetClusterCacheLimit::execute_internal(XmlRpc::XmlRpcValue& params,
                                       XmlRpc::XmlRpcValue& result)
{
    with_api_exception_conversion([&]()
    {
        const std::string volname(getID(params[0]));
        boost::optional<vd::ClusterCount>
            limit(api::getClusterCacheLimit(vd::VolumeId(volname)));

        if(limit)
        {
            result[XMLRPCKeys::cluster_cache_limit] = XMLVAL(*limit);
        }
    });
}

void
ListClusterCacheHandles::execute_internal(::XmlRpc::XmlRpcValue& /* params */,
                                          ::XmlRpc::XmlRpcValue& result)
{
    // AR: funnel through a yet to be created API call? Or rather clean up all similar
    // API calls?
    vd::ClusterCache& cc = vd::VolManager::get()->getClusterCache();
    const std::vector<vd::ClusterCacheHandle> handles(cc.list_namespaces());
    result.clear();
    result.setSize(0);
    size_t i = 0;

    for (const auto& h : handles)
    {
        result[i] = boost::lexical_cast<std::string>(h);
        ++i;
    }
}

void
GetClusterCacheHandleInfo::execute_internal(XmlRpc::XmlRpcValue& params,
                                         XmlRpc::XmlRpcValue& result)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::cluster_cache_handle);
    const std::string s(params[0][XMLRPCKeys::cluster_cache_handle]);
    const auto handle(boost::lexical_cast<vd::ClusterCacheHandle>(s));

    vd::ClusterCache& cc = vd::VolManager::get()->getClusterCache();
    const vd::ClusterCache::NamespaceInfo info(cc.namespace_info(handle));

    const XMLRPCClusterCacheHandleInfo xinfo(info.handle,
                                             info.entries,
                                             info.max_entries,
                                             info.map_stats);

    result = XMLRPCStructs::serialize_to_xmlrpc_value(xinfo);
}

void
RemoveClusterCacheHandle::execute_internal(XmlRpc::XmlRpcValue& params,
                                           XmlRpc::XmlRpcValue& /* result */)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::cluster_cache_handle);
    const std::string s(params[0][XMLRPCKeys::cluster_cache_handle]);
    const auto handle(boost::lexical_cast<vd::ClusterCacheHandle>(s));

    vd::ClusterCache& cc = vd::VolManager::get()->getClusterCache();
    cc.remove_namespace(handle);
}

void
MigrateObject::execute_internal(::XmlRpc::XmlRpcValue& params,
                                ::XmlRpc::XmlRpcValue& /*result*/)
{
    const ObjectId id(getID(params[0]).str());
    const auto force = XMLRPCUtils::get_boolean_enum<ForceRestart>(params[0],
                                                                   XMLRPCKeys::force);
    fs_.object_router().migrate(id,
                                force);
}

void
RestartObject::execute_internal(::XmlRpc::XmlRpcValue& params,
                                ::XmlRpc::XmlRpcValue& /*result*/)
{
    const ObjectId id(getID(params[0]).str());
    const auto force = XMLRPCUtils::get_boolean_enum<ForceRestart>(params[0],
                                                                   XMLRPCKeys::force);

    fs_.object_router().restart(id,
                                force);
}

void
StopObject::execute_internal(::XmlRpc::XmlRpcValue& params,
                             ::XmlRpc::XmlRpcValue& /* result */)
{
    const ObjectId id(getID(params[0]).str());
    const auto delete_local_data =
        XMLRPCUtils::get_boolean_enum<vd::DeleteLocalData>(params[0],
                                                           XMLRPCKeys::delete_local_data);
    fs_.object_router().stop(id,
                             delete_local_data);
}

void
VolumesOverview::execute_internal(::XmlRpc::XmlRpcValue&,
                                  ::XmlRpc::XmlRpcValue& result)
{
    std::map<vd::VolumeId, vd::VolumeOverview> the_map;
    api::getVolumesOverview(the_map);

    for (const auto& it : the_map)
    {
        ::XmlRpc::XmlRpcValue val;
        val[XMLRPCKeys::state] = ::XmlRpc::XmlRpcValue(vd::VolumeOverview::getStateString(it.second.state_));
        val[XMLRPCKeys::nspace] = ::XmlRpc::XmlRpcValue(it.second.ns_.str());
        result[it.first] = val;
    }
}

void
VolumeDestroy::execute_internal(::XmlRpc::XmlRpcValue& params,
                                ::XmlRpc::XmlRpcValue& /*result*/)
{
    const vd::VolumeId volName(getID(params[0]));
    const vd::DeleteLocalData delete_local_data =
        XMLRPCUtils::get_boolean_enum<vd::DeleteLocalData>(params[0],
                                            XMLRPCKeys::delete_local_data);
    const vd::ForceVolumeDeletion force_volume_deletion =
        XMLRPCUtils::get_boolean_enum<vd::ForceVolumeDeletion>(params[0],
                                                XMLRPCKeys::force);
    const vd::RemoveVolumeCompletely remove_volume_completely =
        XMLRPCUtils::get_boolean_enum<vd::RemoveVolumeCompletely>(params[0],
                                                   XMLRPCKeys::delete_global_metadata);
    // const vd::DeleteVolumeNamespace delete_volume_namespace =
    //     XMLRPCUtils::get_boolean_enum<vd::RemoveVolumeCompletely>(params[0],
    //                                                XMLRPCKeys::delete_volume_namespace);

    auto delete_volume_namespace = T(remove_volume_completely) ? vd::DeleteVolumeNamespace::T : vd::DeleteVolumeNamespace::F;

    api::destroyVolume(volName,
                       delete_local_data,
                       remove_volume_completely,
                       delete_volume_namespace,
                       force_volume_deletion);
    }

void
SetGeneralLoggingLevel::execute_internal(XmlRpc::XmlRpcValue& params,
                                         XmlRpc::XmlRpcValue& /* result */)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::log_filter_level);
    const std::string level(params[0][XMLRPCKeys::log_filter_level]);
    yt::Severity sev = yt::Severity::info;

    try
    {
        sev = boost::lexical_cast<yt::Severity>(level);
    }
    catch (std::exception& e)
    {
        throw XmlRpc::XmlRpcException("could not read the severity arg " + level);
    }

    youtils::Logger::generalLogging(sev);
}

void
GetGeneralLoggingLevel::execute_internal(XmlRpc::XmlRpcValue& /*params*/,
                                         XmlRpc::XmlRpcValue&  result)
{
    const yt::Severity level(youtils::Logger::generalLogging());
    result = XMLVAL(boost::lexical_cast<std::string>(level));
}

void
ShowLoggingFilters::execute_internal(XmlRpc::XmlRpcValue& /*params*/,
                                     XmlRpc::XmlRpcValue&  result)
{
    std::vector<youtils::Logger::filter_t> filters;
    youtils::Logger::all_filters(filters);

    result = XmlRpcArray();
    unsigned i = 0;

    for(const youtils::Logger::filter_t& filter : filters)
    {
        XmlRpc::XmlRpcValue entry;
        entry[XMLRPCKeys::log_filter_name] = filter.first;
        entry[XMLRPCKeys::log_filter_level] =
            boost::lexical_cast<std::string>(filter.second);

        result[i++] = entry;
    }
}

void
AddLoggingFilter::execute_internal(XmlRpc::XmlRpcValue& params,
                                   XmlRpc::XmlRpcValue& /* result */)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::log_filter_name);
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::log_filter_level);
    const std::string name(params[0][XMLRPCKeys::log_filter_name]);
    const std::string level(params[0][XMLRPCKeys::log_filter_level]);

    yt::Severity sev = yt::Severity::info;

    try
    {
        sev = boost::lexical_cast<yt::Severity>(level);
    }
    catch (std::exception& e)
    {
        throw XmlRpc::XmlRpcException("could not read the severity arg " + level);
    }

    youtils::Logger::add_filter(name, sev);
}

void
RemoveLoggingFilter::execute_internal(XmlRpc::XmlRpcValue& params,
                                      XmlRpc::XmlRpcValue& /* result */)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::log_filter_name);
    const std::string name(params[0][XMLRPCKeys::log_filter_name]);
    youtils::Logger::remove_filter(name);
}

void
RemoveAllLoggingFilters::execute_internal(XmlRpc::XmlRpcValue& /*params*/,
                                          XmlRpc::XmlRpcValue& /* result */)
{
    youtils::Logger::remove_all_filters();
}

void
GetVolumeId::execute_internal(XmlRpc::XmlRpcValue& params,
                              XmlRpc::XmlRpcValue& result)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::target_path);
    const FrontendPath path(static_cast<const std::string&>(params[0][XMLRPCKeys::target_path]));
    const boost::optional<vd::VolumeId> maybe_id(fs_.get_volume_id(path));

    if (maybe_id)
    {
        result[XMLRPCKeys::volume_id] = *maybe_id;
    }
    else
    {
        result[XMLRPCKeys::volume_id] = "";
    }
}

void
GetObjectId::execute_internal(XmlRpc::XmlRpcValue& params,
                              XmlRpc::XmlRpcValue& result)
{
    XMLRPCUtils::ensure_arg(params[0], XMLRPCKeys::target_path);
    const FrontendPath path(static_cast<const std::string&>(params[0][XMLRPCKeys::target_path]));
    const boost::optional<ObjectId> maybe_id(fs_.get_object_id(path));

    if (maybe_id)
    {
        result[XMLRPCKeys::object_id] = *maybe_id;
    }
    else
    {
        result[XMLRPCKeys::object_id] = "";
    }
}

void
UpdateClusterNodeConfigs::execute_internal(::XmlRpc::XmlRpcValue& /* params */,
                                     ::XmlRpc::XmlRpcValue& /*result*/)
{
    fs_.object_router().update_cluster_node_configs();
}

void
VAAICopy::execute_internal(XmlRpc::XmlRpcValue& params,
                           XmlRpc::XmlRpcValue& result)
{
    const FrontendPath src_path(static_cast<const std::string&>
           (params[0][XMLRPCKeys::src_path]));
    const FrontendPath dst_path(static_cast<const std::string&>
            (params[0][XMLRPCKeys::target_path]));
    const uint64_t timeout(getUIntVal<uint64_t>(params[0][XMLRPCKeys::timeout]));
    const CloneFileFlags flags(static_cast<CloneFileFlags>
            (getUIntVal<unsigned int>(params[0][XMLRPCKeys::flags])));

    fs_.vaai_copy(src_path,
                  dst_path,
                  timeout,
                  flags);

    const boost::optional<ObjectId> dst_id(fs_.find_id(dst_path));
    uint64_t dst_filesize = fs_.object_router().get_size(*dst_id);
    result[XMLRPCKeys::volume_size] =
        XmlRpc::XmlRpcValue(std::to_string(dst_filesize));
}

void
GetFailOverCacheConfigMode::execute_internal(::XmlRpc::XmlRpcValue& params,
                                             ::XmlRpc::XmlRpcValue& result)
{
    with_api_exception_conversion([&]
    {
        ObjectId oid(getID(params[0]));
        const FailOverCacheConfigMode
            mode = fs_.object_router().get_foc_config_mode(oid);

        result[XMLRPCKeys::foc_config_mode] = boost::lexical_cast<std::string>(mode);
    });
}

void
GetFailOverCacheConfig::execute_internal(::XmlRpc::XmlRpcValue& params,
                                         ::XmlRpc::XmlRpcValue& result)
{
    with_api_exception_conversion([&]
    {
        const vd::VolumeId volName(getID(params[0]));
        const boost::optional<vd::FailOverCacheConfig>
            foc_config(api::getFailOverCacheConfig(volName));
        if (foc_config)
        {
            std::stringstream ss;
            boost::archive::text_oarchive oa(ss);
            oa << *foc_config;
            result[XMLRPCKeys::foc_config] = ss.str();
        }
    });
}

void
SetManualFailOverCacheConfig::execute_internal(::XmlRpc::XmlRpcValue& params,
                                               ::XmlRpc::XmlRpcValue& /* result */)
{
    with_api_exception_conversion([&]
    {
        auto param = params[0];
        const ObjectId oid(getID(param));
        boost::optional<vd::FailOverCacheConfig> foc_config;
        if (param.hasMember(XMLRPCKeys::foc_config))
        {
            std::stringstream ss(param[XMLRPCKeys::foc_config]);
            vd::FailOverCacheConfig fc("",
                                       0,
                                       vd::DtlMode::Asynchronous);
            boost::archive::text_iarchive ia(ss);
            ia >> fc;
            foc_config = fc;
        }
        fs_.object_router().set_manual_foc_config(oid, foc_config);
    });
}

void
SetAutomaticFailOverCacheConfig::execute_internal(::XmlRpc::XmlRpcValue& params,
                                                  ::XmlRpc::XmlRpcValue& /* result */)
{
    with_api_exception_conversion([&]
    {
        auto param = params[0];
        const ObjectId oid(getID(param));
        fs_.object_router().set_automatic_foc_config(oid);
    });
}

void
GetMetaDataCacheCapacity::execute_internal(::XmlRpc::XmlRpcValue& params,
                                           ::XmlRpc::XmlRpcValue& result)
{
    const vd::VolumeId vol_id(getID(params[0]));

    with_api_exception_conversion([&]
    {
        const boost::optional<size_t>
            cap(api::getVolumeConfig(vol_id).metadata_cache_capacity_);

        ensureStruct(result);
        if (cap)
        {
            result[XMLRPCKeys::metadata_cache_capacity] = XMLVAL(*cap);
        }
    });
}

void
SetMetaDataCacheCapacity::execute_internal(::XmlRpc::XmlRpcValue& params,
                                           ::XmlRpc::XmlRpcValue& /*result*/)
{
    with_api_exception_conversion([&]
    {
        auto param = params[0];
        const vd::VolumeId vol_id(getID(param));
        boost::optional<size_t> cap;
        if (param.hasMember(XMLRPCKeys::metadata_cache_capacity))
        {
            const std::string s(param[XMLRPCKeys::metadata_cache_capacity]);
            cap = boost::lexical_cast<size_t>(s);
        }
        api::setMetaDataCacheCapacity(vol_id,
                                      cap);
    });
}

}
