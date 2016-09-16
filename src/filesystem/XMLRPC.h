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

#ifndef _VOLUMEDRIVERFS_XMLRPC_H_
#define _VOLUMEDRIVERFS_XMLRPC_H_

#include <limits>

#include <loki/Typelist.h>
#include <loki/TypelistMacros.h>

#include <boost/lexical_cast.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <xmlrpc++0.7/src/Server.h>
#include <xmlrpc++0.7/src/XmlRpc.h>

#include <youtils/Logging.h>

#include <volumedriver/VolumeConfig.h>
#include <volumedriver/Types.h>

#include <filesystem/ExtraLokiTypelistMacros.h>

namespace volumedriverfs
{

enum class XMLRPCErrorCode
{
    ObjectNotFound = 1,
    InvalidOperation = 2,
    SnapshotNotFound = 3,
    FileExists = 4,
    InsufficientResources = 5,
    PreviousSnapshotNotOnBackend = 6,
    ObjectStillHasChildren = 7,
    SnapshotNameAlreadyExists = 8,
};

class FileSystem;

#define REGISTER_XMLRPC(basetype, classname, methodname, helpstring)   \
    class classname                                                    \
        : public basetype                                              \
    {                                                                  \
    public:                                                            \
        classname(::xmlrpc::ServerBase& s,                             \
                  FileSystem& fs)                                      \
            : basetype(methodname, s, fs)                              \
        {}                                                             \
                                                                       \
        virtual void                                                   \
        execute_internal(::XmlRpc::XmlRpcValue &params,                \
                         ::XmlRpc::XmlRpcValue &result);               \
                                                                       \
        std::string                                                    \
        help()                                                         \
        {                                                              \
            return std::string(helpstring);                            \
        }                                                              \
                                                                       \
        static const char*                                             \
        method_name()                                                  \
        {                                                              \
            static const char n[] = methodname;                        \
            return n;                                                  \
        }                                                              \
    }

class XMLRPCCallBasic
    : public ::XmlRpc::XmlRpcServerMethod
{
public:
    XMLRPCCallBasic(const std::string& name,
                    ::xmlrpc::ServerBase& s,
                    FileSystem& fs)
        : ::XmlRpc::XmlRpcServerMethod(name, &s)
        , fs_(fs)
    {}

    virtual void
    execute(::XmlRpc::XmlRpcValue& params,
            ::XmlRpc::XmlRpcValue& result);

    template<typename T>
    T
    static
    getUIntVal(::XmlRpc::XmlRpcValue& xval)
    {
        std::string v(xval);
        char* tail;
        uint64_t val = strtoull(v.c_str(),
                                &tail,
                                10);
        if(val == 0 and tail != (v.c_str() + v.size()))
        {
            throw ::XmlRpc::XmlRpcException("Could not convert " + v + " to unsigned int", 1);
        }
        if(val > std::numeric_limits<T>::max())
        {
            throw ::XmlRpc::XmlRpcException("Argument too large " + v, 1);
        }
        return static_cast<T>(val);
    }

protected:
    DECLARE_LOGGER("XMLRPCCall");

    FileSystem& fs_;
    std::string currentarg;

    virtual void
    execute_internal(::XmlRpc::XmlRpcValue& params,
                     ::XmlRpc::XmlRpcValue& result) = 0;

    bool
    getbool(::XmlRpc::XmlRpcValue& param, const std::string& name);

    bool
    getboolWithDefault(::XmlRpc::XmlRpcValue& param,
                       const std::string& name,
                       const bool def);

    void
    ensureStruct(::XmlRpc::XmlRpcValue& val);

    template<typename T>
    ::XmlRpc::XmlRpcValue
    XMLVAL(const T& val)
    {
        return ::XmlRpc::XmlRpcValue(boost::lexical_cast<std::string>(val));
    }

    ::XmlRpc::XmlRpcValue
    XMLVAL(uint8_t val);

    ::XmlRpc::XmlRpcValue
    XMLVAL(int8_t val);

    ::XmlRpc::XmlRpcValue
    XMLVAL(bool val);

    volumedriver::VolumeId
    getID(::XmlRpc::XmlRpcValue& param);

    std::string
    getSnapID(::XmlRpc::XmlRpcValue& param);

    std::string
    getScrubbingWorkResult(::XmlRpc::XmlRpcValue& param);

    volumedriver::Namespace
    getNS(::XmlRpc::XmlRpcValue& param);

    uint64_t
    getVOLSIZE(::XmlRpc::XmlRpcValue& param);

    uint64_t
    getMINSCO(::XmlRpc::XmlRpcValue& param);

    uint64_t
    getMAXSCO(::XmlRpc::XmlRpcValue& param);

    uint64_t
    getMAXNDSCO(::XmlRpc::XmlRpcValue& param);

    volumedriver::SCOMultiplier
    getSCOMULT(::XmlRpc::XmlRpcValue& param);

    volumedriver::LBASize
    getLBASIZE(::XmlRpc::XmlRpcValue& param);

    volumedriver::ClusterMultiplier getCLUSTERMULT(::XmlRpc::XmlRpcValue& param);

    volumedriver::Namespace getPAR_NS(::XmlRpc::XmlRpcValue& param);
    std::string getPAR_SS(::XmlRpc::XmlRpcValue& param);

    volumedriver::PrefetchVolumeData
    getPREFETCH(::XmlRpc::XmlRpcValue& param);

    uint64_t getDelay(::XmlRpc::XmlRpcValue& param);

    static void
    setError(::XmlRpc::XmlRpcValue& result,
             const XMLRPCErrorCode error_code,
             const char* error_string = "");
};

#define XMLRPC_WRAPPER(classname)                  \
template<class T>                                  \
class classname : public T                         \
{                                                  \
public:                                            \
    using T::T;                                    \
                                                   \
    virtual ~classname () {}                       \
                                                   \
    virtual void                                   \
    execute(::XmlRpc::XmlRpcValue& params,         \
            ::XmlRpc::XmlRpcValue& result);        \
                                                   \
    DECLARE_LOGGER(#classname);                    \
};                                                 \

XMLRPC_WRAPPER(XMLRPCTimingWrapper);
XMLRPC_WRAPPER(XMLRPCLockWrapper);
XMLRPC_WRAPPER(XMLRPCRedirectWrapper);

typedef XMLRPCTimingWrapper<XMLRPCLockWrapper<XMLRPCCallBasic> > XMLRPCCallTimingLock;
typedef XMLRPCTimingWrapper<XMLRPCCallBasic> XMLRPCCallTiming;
typedef XMLRPCTimingWrapper<XMLRPCRedirectWrapper<XMLRPCCallBasic> > XMLRPCCallTimingRedirect;
typedef XMLRPCTimingWrapper<XMLRPCRedirectWrapper<XMLRPCLockWrapper<XMLRPCCallBasic> > > XMLRPCCallTimingRedirectLock;

template<class TList>
struct InstantiateXMLRPCS;

template<>
struct InstantiateXMLRPCS< ::Loki::NullType>
{
    static void
    doit(::xmlrpc::Server& /* s */,
         FileSystem& /* fs */)
    {}
};

template<class H, class T>
struct InstantiateXMLRPCS< ::Loki::Typelist<H, T> >
{
    static void
    doit(::xmlrpc::Server& s,
         FileSystem& fs)
    {
        std::unique_ptr<::XmlRpc::XmlRpcServerMethod> m(new H(*s.getServer(), fs));
        s.addMethod(std::move(m));
        InstantiateXMLRPCS<T>::doit(s, fs);
    }
};

// ================== EXPOSED IN XMLRPC CLIENT ===================

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                VolumeCreate,
                "volumeCreate",
                "Create a volume");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                VolumesList,
                "volumesList",
                "Get a list of volumes");

REGISTER_XMLRPC(XMLRPCCallTiming,
                VolumesListByPath,
                "volumesListByPath",
                "Get a list of volumes by path");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                Unlink,
                "unlink",
                "Unlink a file");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                UpdateMetaDataBackendConfig,
                "updateMetaDataBackendConfig",
                "Update a volume's MetaDataBackendConfig");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                VolumeInfo,
                "volumeInfo",
                "Show information about a volume");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SnapshotCreate,
                "snapshotCreate",
                "Create a snapshot");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SnapshotsList,
                "snapshotsList",
                "List snapshots");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SnapshotInfo,
                "snapshotInfo",
                "Show a snapshot");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                VolumeClone,
                "volumeClone",
                "Clone from a volume");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                SnapshotRestore,
                "snapshotRestore",
                "Restore a snapshot");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                SnapshotDestroy,
                "snapshotDestroy",
                "Remove a snapshot");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                VolumePerformanceCounters,
                "volumePerformanceCounters",
                "Performance data");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                VolumeDriverPerformanceCounters,
                "volumeDriverPerformanceCounters",
                "Performance data");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                SetVolumeAsTemplate,
                "setVolumeAsTemplate",
                "convert a volume to a template");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                GetScrubbingWork,
                "getScrubbingWork",
                "Get scrubbing work");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                ResizeObject,
                "resizeObject",
                "Resize an object");

REGISTER_XMLRPC(XMLRPCCallTiming,
                ApplyScrubbingResult,
                "applyScrubbingResult",
                "Apply scrubbing work");

REGISTER_XMLRPC(XMLRPCCallTiming,
                Revision,
                "revision",
                "Returns the VolumeDriver revision number");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                VolumePotential,
                "volumePotential",
                "Returns the number of additional volumes the node can host");

//No redirection here obviously
REGISTER_XMLRPC(XMLRPCCallTiming,
                MarkNodeOffline,
                "markNodeOffline",
                "marks a node as offline");

//No redirection here obviously
REGISTER_XMLRPC(XMLRPCCallTiming,
                MarkNodeOnline,
                "markNodeOnline",
                "marks a node as online");

//No redirection here obviously
REGISTER_XMLRPC(XMLRPCCallTiming,
                GetNodesStatusMap,
                "getNodesStatusMap",
                "gets the configured nodes and their status");

//No redirection here obviously
REGISTER_XMLRPC(XMLRPCCallTimingLock,
                GetVolumeId,
                "getVolumeId",
                "returns the volume ID for a given path (if backed by a volume)");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                GetObjectId,
                "getObjectId",
                "returns the object ID for a given path (volume or file)");

//No redirection here - we only want to operate on the local node
REGISTER_XMLRPC(XMLRPCCallTimingLock,
                PersistConfigurationToString,
                "persistConfigurationToString",
                "persist configuration to a string");

//No redirection here - we only want to operate on the local node
REGISTER_XMLRPC(XMLRPCCallTimingLock,
                UpdateConfiguration,
                "updateConfiguration",
                "update the configuration from the given file");

REGISTER_XMLRPC(XMLRPCCallTiming,
                SetGeneralLoggingLevel,
                "setGeneralLoggingLevel",
                "set the general logging level");

REGISTER_XMLRPC(XMLRPCCallTiming,
                GetGeneralLoggingLevel,
                "getGeneralLoggingLevel",
                "get the general logging level");

REGISTER_XMLRPC(XMLRPCCallTiming,
                ShowLoggingFilters,
                "showLoggingFilters",
                "show the logging filters");

REGISTER_XMLRPC(XMLRPCCallTiming,
                AddLoggingFilter,
                "addLoggingFilter",
                "add a logging filter");

REGISTER_XMLRPC(XMLRPCCallTiming,
                RemoveLoggingFilter,
                "removeLoggingFilter",
                "remove a logging filter");

REGISTER_XMLRPC(XMLRPCCallTiming,
                RemoveAllLoggingFilters,
                "removeAllLoggingFilters",
                "remove all logging filters");

REGISTER_XMLRPC(XMLRPCCallTiming,
                MallocInfo,
                "mallocInfo",
                "get malloc_info output");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                MigrateObject,
                "migrateVolume",
                "migrate an object to a specific node");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                RestartObject,
                "restartVolume",
                "restart an object on its owning node");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                StopObject,
                "stopVolume",
                "stop an object");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                UpdateClusterNodeConfigs,
                "updateClusterNodeConfigs",
                "re-read the ClusterNodeConfigs from the ClusterRegistry");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetFailOverCacheConfigMode,
                "getFailOverCacheConfigMode",
                "Returns the configuration mode of the DTL");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetFailOverCacheConfig,
                "getFailOverCacheConfig",
                "Returns the configuration of the DTL on the volume");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SetAutomaticFailOverCacheConfig,
                "setAutomaticFailOverCacheConfig",
                "Set automatic DTL configuration");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SetManualFailOverCacheConfig,
                "setManualFailOverCacheConfig",
                "Set manual DTL configuration");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                IsVolumeSyncedUpToTLog,
                "isVolumeSyncedUpToTLog",
                "Checks whether the volume is synced or not up to a certain TLog");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                IsVolumeSyncedUpToSnapshot,
                "isVolumeSyncedUpToSnapshot",
                "Returns whether the volume is synced up to a particular snapshot");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                ScheduleBackendSync,
                "scheduleBackendSync",
                "Closes current SCO and TLog and schedules them for write to backend");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                ListClientConnections,
                "ListClientConnections",
                "List client connections");

// ================== NOT EXPOSED, NOT TESTED   ==================

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                DataStoreWriteUsed,
                "dataStoreWriteUsed",
                "Disk usage of Datastore data not yet in the backend");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                DataStoreReadUsed,
                "dataStoreReadUsed",
                "Disk usage of Datastore data already in the backend");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                QueueCount,
                "queueCount",
                "Returns the number of SCOS in a queue");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                QueueSize,
                "queueSize",
                "Returns the total size of SCO's waiting to be written to the backend");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                TLogUsed,
                "tlogUsed",
                "Returns total size used by the tlogs");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                ScoCacheInfo,
                "scoCacheInfo",
                "Return disk usage information of the SCO cache");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                VolumeScoCacheInfo,
                "volumeScoCacheInfo",
                "Return disk usage information of the SCO cache");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                RemoveNamespaceFromSCOCache,
                "removeNamespaceFromSCOCache",
                "Removes a namespace from the SCO Cache");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                GetFailOverMode,
                "getFailoverMode",
                "Returns current failover mode of a volume");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                SnapshotSCOCount,
                "snapshotSCOCount",
                "Counts the number of scos in a snapshot");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                CurrentSCOCount,
                "currentSCOCount",
                "Counts the number of scos after the last snapshot");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                CheckVolumeConsistency,
                "checkVolumeConsistency",
                "checks whether an offline volume is consistent");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                SnapshotBackendSize,
                "getSnapshotBackendSize",
                "get an estimate of the size of this snapshot on the backend");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                CurrentBackendSize,
                "getCurrentBackendSize",
                "get an estimate of the size of the currrent data on the backend");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                DumpSCOInfo,
                "dumpSCOInfo",
                "dump SCOCache SCO info");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                AddSCOCacheMountPoint,
                "addSCOCacheMountPoint",
                "add a mountpoint to the SCO cache");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                RemoveSCOCacheMountPoint,
                "removeSCOCacheMountPoint",
                "remove a mountpoint from the SCO cache");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                StartPrefetching,
                "startPrefetching",
                "start prefetching data for the volume");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                CheckConfiguration,
                "checkConfiguration",
                "check configuration from the file");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetVolumeDriverState,
                "getVolumeDriverState",
                "get the current state of the volumedriver");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SetClusterCacheBehaviour,
                "setClusterCacheBehaviour",
                "set the cluster cache behaviour of the volume to one of CacheOnWrite, CacheOnRead or NoCache or back to the default for this volumedriver");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetClusterCacheBehaviour,
                "getClusterCacheBehaviour",
                "get the cluster cache behaviour of the volume -- no result means default volumedriver behaviour");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SetClusterCacheMode,
                "setClusterCacheMode",
                "set the cluster cache mode of the volume to one of ContentBased or LocationBased");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetClusterCacheMode,
                "getClusterCacheMode",
                "get the cluster cache behaviour of the volume -- no result means default volumedriver behaviour");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SetClusterCacheLimit,
                "setClusterCacheLimit",
                "set the cluster cache limit of the volume - no value means unlimited");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetClusterCacheLimit,
                "getClusterCacheLimit",
                "get the cluster cache limit of the volume -- no result means no limit");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                ListClusterCacheHandles,
                "listClusterCacheHandles",
                "get a list of registered ClusterCache handles");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                GetClusterCacheHandleInfo,
                "getClusterCacheHandleInfo",
                "request detailed info about a ClusterCache handle");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                RemoveClusterCacheHandle,
                "removeClusterCacheHandle",
                "remove (deregister) a ClusterCache handle");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                VolumesOverview,
                "volumesOverview",
                "get more info on active and restarting volumes");

REGISTER_XMLRPC(XMLRPCCallTimingLock,
                VolumeDestroy,
                "volumeDestroy",
                "Destroy a volume");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SetSyncIgnore,
                "setSyncIgnore",
                "set the number of syncs to ignore and a timeout to stop ignoring");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetSyncIgnore,
                "getSyncIgnore",
                "get the sync ignore settings");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SetSCOMultiplier,
                "setSCOMultiplier",
                "set the number of clusters in a SCO");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetSCOMultiplier,
                "getSCOMultiplier",
                "get the number of clusters in a SCO");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SetTLogMultiplier,
                "setTLogMultiplier",
                "set the number of SCO's in a TLOG");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetTLogMultiplier,
                "getTLogMultiplier",
                "get the number of SCO's in a TLOG");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SetSCOCacheMaxNonDisposableFactor,
                "setSCOCacheMaxNonDisposableFactor",
                "set the factor of non-disposable data");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetSCOCacheMaxNonDisposableFactor,
                "getSCOCacheMaxNonDisposableFactor",
                "get the factor of non-disposable data");

REGISTER_XMLRPC(XMLRPCCallTimingRedirect,
                VAAICopy,
                "vaaiCopy",
                "VAAI support");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                SetMetaDataCacheCapacity,
                "setMetaDataCacheCapacity",
                "set capacity of the metadata cache (in pages)");

REGISTER_XMLRPC(XMLRPCCallTimingRedirectLock,
                GetMetaDataCacheCapacity,
                "getMetaDataCacheCapacity",
                "get capacity of the metadata cache (in pages)");

typedef LOKI_TYPELIST_86(
// ================== EXPOSED IN XMLRPC CLIENT ===================
                         VolumeCreate,
                         VolumesList,
                         VolumesListByPath,
                         VolumeInfo,
                         Unlink,
                         SnapshotCreate,
                         SnapshotsList,
                         SnapshotInfo,
                         VolumeClone,
                         UpdateMetaDataBackendConfig,
                         SnapshotRestore,
                         SnapshotDestroy,
                         IsVolumeSyncedUpToSnapshot,
                         VolumePerformanceCounters,
                         VolumeDriverPerformanceCounters,
                         SetVolumeAsTemplate,
                         GetScrubbingWork,
                         ApplyScrubbingResult,
                         Revision,
                         MarkNodeOffline,
                         MarkNodeOnline,
                         GetNodesStatusMap,
                         PersistConfigurationToString,
                         UpdateConfiguration,
                         GetObjectId,
                         GetVolumeId,
                         SetGeneralLoggingLevel,
                         GetGeneralLoggingLevel,
                         ShowLoggingFilters,
                         AddLoggingFilter,
                         RemoveLoggingFilter,
                         RemoveAllLoggingFilters,
                         MallocInfo,
                         MigrateObject,
                         RestartObject,
                         StopObject,
                         VolumePotential,
                         SetClusterCacheBehaviour,
                         GetClusterCacheBehaviour,
                         SetClusterCacheMode,
                         GetClusterCacheMode,
                         SetClusterCacheLimit,
                         GetClusterCacheLimit,
                         ListClusterCacheHandles,
                         GetClusterCacheHandleInfo,
                         RemoveClusterCacheHandle,
                         GetSyncIgnore,
                         SetSyncIgnore,
                         GetSCOMultiplier,
                         SetSCOMultiplier,
                         GetTLogMultiplier,
                         SetTLogMultiplier,
                         GetMetaDataCacheCapacity,
                         SetMetaDataCacheCapacity,
                         GetSCOCacheMaxNonDisposableFactor,
                         SetSCOCacheMaxNonDisposableFactor,
                         UpdateClusterNodeConfigs,
                         GetFailOverCacheConfigMode,
                         GetFailOverCacheConfig,
                         SetAutomaticFailOverCacheConfig,
                         SetManualFailOverCacheConfig,
                         IsVolumeSyncedUpToTLog,
                         ScheduleBackendSync,
                         VAAICopy,
                         ListClientConnections,
                         ResizeObject,
                         // ================== NOT EXPOSED, NOT TESTED   ==================
                         GetFailOverMode,
                         ScoCacheInfo,
                         VolumeScoCacheInfo,
                         RemoveNamespaceFromSCOCache,
                         VolumeDestroy,
                         // These are not supposed to be executed via xmlrpc but only
                         // (implicitly) via the filesystem interface.
                         // VolumeGetWork,
                         // VolumePutWorkOutput,
                         DataStoreWriteUsed,
                         DataStoreReadUsed,
                         QueueCount,
                         QueueSize,
                         TLogUsed,
                         SnapshotSCOCount,
                         CurrentSCOCount,
                         CheckVolumeConsistency,
                         SnapshotBackendSize,
                         CurrentBackendSize,
                         DumpSCOInfo,
                         StartPrefetching,
                         CheckConfiguration,
                         GetVolumeDriverState,
                         VolumesOverview
                         ) xmlrpcs;

#undef REGISTER_XMLRPC
#undef XMLRPC_WRAPPER
}

#endif // _VOLUMEDRIVERFS_XMLRPC_H_

// Local Variables: **
// mode: c++ **
// End: **
