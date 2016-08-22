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

#include "ArakoonClient.h"
#include "ChronoDurationConverter.h"
#include "FileSystemMetaDataClient.h"
#include "IterableConverter.h"
#include "LocalClient.h"
#include "LockedClient.h"
#include "MDSClient.h"
#include "ObjectRegistryClient.h"
#include "OptionalConverter.h"
#include "Piccalilli.h"
#include "PythonTestHelpers.h"
#include "ScrubManagerClient.h"
#include "StringyConverter.h"
#include "StrongArithmeticTypedefConverter.h"

#include <iostream>

#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>

#include <boost/python/class.hpp>
#include <boost/python/copy_const_reference.hpp>
#include <boost/python/def.hpp>
#include <boost/python/enum.hpp>
#include <boost/python/module.hpp>
#include <boost/python/exception_translator.hpp>
#include <boost/python/object.hpp>
#include <boost/python/register_ptr_to_python.hpp>
#include <boost/python/return_value_policy.hpp>

#include <boost/serialization/export.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <youtils/Logger.h>
#include <youtils/LoggerToolCut.h>
#include <youtils/LoggingToolCut.h>
#include <youtils/PythonBuildInfo.h>
#include <youtils/UpdateReport.h>

#include <volumedriver/ClusterCount.h>
#include <volumedriver/MDSNodeConfig.h>
#include <volumedriver/MetaDataBackendConfig.h>
#include <volumedriver/PythonScrubber.h>

#include <filesystem/ClusterId.h>
#include <filesystem/ClusterRegistry.h>
#include <filesystem/FailOverCacheConfigMode.h>
#include <filesystem/LockedPythonClient.h>
#include <filesystem/NodeId.h>
#include <filesystem/Object.h>
#include <filesystem/PythonClient.h>
#include <filesystem/XMLRPCKeys.h>

// The "storagerouterclient" name is inherited from the previous python implementation
// - find a better name before it's becoming too entrenched / while renaming is still cheap.

namespace ara = arakoon;
namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace vfs = volumedriverfs;
namespace vfspy = volumedriverfs::python;
namespace yt = youtils;

using namespace std::literals::string_literals;

// Some issues with exception propagation from c++ to python.
//
// The boost-python documentation only addresses changing the message for
// existing exception types (PyExc_RuntimeError, ...), it does not address how to define /raise
// custom exceptions in python.
//
// From here a strategy is gotten to raise custom exceptions to python:
// http://stackoverflow.com/questions/2261858/boostpython-export-custom-exception
// Unfortunately, however, these exceptions don't derive from Exception (in python), which means that
// if you don't catch them correctly you get no useful information at at all:
//
//       In [4]: c.test_exceptions(1)
//       ---------------------------------------------------------------------------
//       SystemError                               Traceback (most recent call last)
//       SystemError: 'finally' pops bad exception
//
// To get python exceptions deriving from Exception we followed the recipe described here:
// http://stackoverflow.com/questions/9620268/boost-python-custom-exception-class
// The disadvantage of this strategy is now that the exception in python does not originate
// from a C++ class so it's not clear how we could add properties to it.

#define PYTHON_EXCEPTION_TYPE(name) name##_pythontype
#define EXCEPTION_TRANSLATOR_NAME(name) translate_##name

#define DEFINE_EXCEPTION_TRANSLATOR(name)                               \
    PyObject *PYTHON_EXCEPTION_TYPE(name) = NULL;                       \
    void EXCEPTION_TRANSLATOR_NAME(name)(vfs::clienterrors::name const& e) \
    {                                                                   \
        ASSERT(PYTHON_EXCEPTION_TYPE(name) != NULL);                    \
        PyErr_SetString(PYTHON_EXCEPTION_TYPE(name) , e.what());        \
    }

#define REGISTER_EXCEPTION_TRANSLATOR(name)                             \
    PYTHON_EXCEPTION_TYPE(name) = createExceptionClass(#name);          \
    bpy::register_exception_translator<vfs::clienterrors::name>(EXCEPTION_TRANSLATOR_NAME(name))


namespace
{

PyObject*
createExceptionClass(const char* name, PyObject* baseTypeObj = PyExc_Exception)
{
    std::string scopeName = bpy::extract<std::string>(bpy::scope().attr("__name__"));
    std::string qualifiedName0 = scopeName + "." + name;
    char* qualifiedName1 = const_cast<char*>(qualifiedName0.c_str());

    PyObject* typeObj = PyErr_NewException(qualifiedName1, baseTypeObj, 0);
    if(!typeObj) bpy::throw_error_already_set();
    bpy::scope().attr(name) = bpy::handle<>(bpy::borrowed(typeObj));
    return typeObj;
}

DEFINE_EXCEPTION_TRANSLATOR(MaxRedirectsExceededException);
DEFINE_EXCEPTION_TRANSLATOR(ClusterNotReachableException);
DEFINE_EXCEPTION_TRANSLATOR(ObjectNotFoundException);
DEFINE_EXCEPTION_TRANSLATOR(InvalidOperationException);
DEFINE_EXCEPTION_TRANSLATOR(SnapshotNotFoundException);
DEFINE_EXCEPTION_TRANSLATOR(FileExistsException);
DEFINE_EXCEPTION_TRANSLATOR(InsufficientResourcesException);
DEFINE_EXCEPTION_TRANSLATOR(PreviousSnapshotNotOnBackendException);
DEFINE_EXCEPTION_TRANSLATOR(ObjectStillHasChildrenException);
DEFINE_EXCEPTION_TRANSLATOR(SnapshotNameAlreadyExistsException);

void
reminder(vfs::XMLRPCErrorCode code) __attribute__((unused));

void
reminder(vfs::XMLRPCErrorCode code)
{
    // If the compiler yells at you here chances are that you also forgot to add
    // 1) DEFINE_EXCEPTION_TRANSLATOR
    // 2) REGISTER_EXCEPTION_TRANSLATOR
    // 3) documentation on the method(s) which might throw this exceptions

    switch (code)
    {
    case vfs::XMLRPCErrorCode::ObjectNotFound:
    case vfs::XMLRPCErrorCode::InvalidOperation:
    case vfs::XMLRPCErrorCode::SnapshotNotFound:
    case vfs::XMLRPCErrorCode::FileExists:
    case vfs::XMLRPCErrorCode::InsufficientResources:
    case vfs::XMLRPCErrorCode::PreviousSnapshotNotOnBackend:
    case vfs::XMLRPCErrorCode::ObjectStillHasChildren:
    case vfs::XMLRPCErrorCode::SnapshotNameAlreadyExists:
        break;
    }
}

template <typename M>
struct MapToDictConverter
{
    static PyObject*
    convert(const M& map)
    {
        bpy::dict d;
        for (const auto& e : map)
        {
            d[e.first] = e.second;
        }
        return bpy::incref(d.ptr());
    }
};

struct UpdateReportConverter
{
    static PyObject*
    convert(const yt::UpdateReport& report)
    {
        bpy::list l;
        for (const auto& update : report.getUpdates())
        {
            bpy::dict d;
            d[vfs::XMLRPCKeys::param_name] = update.parameter_name;
            d[vfs::XMLRPCKeys::old_value] = update.old_value;
            d[vfs::XMLRPCKeys::new_value] = update.new_value;

            l.append(d);
        }

        return bpy::incref(l.ptr());
    }
};

void
export_debug_module()
{
    bpy::object module(bpy::handle<>(bpy::borrowed(PyImport_AddModule("storagerouterclient._debug"))));
    bpy::scope().attr("_debug") = module;
    bpy::scope scope = module;
    scope.attr("__doc__") = "debugging helpers - this ain't no stable public API!";

    vfspy::FileSystemMetaDataClient::registerize();
    vfspy::ScrubManagerClient::registerize();
}

template<typename T>
std::string
repr(T* t)
{
    std::stringstream ss;
    boost::archive::xml_oarchive oa(ss);
    oa << boost::serialization::make_nvp("XMLRepr",
                                         *t);
    return ss.str();
}

std::string
cluster_cache_handle_repr(vd::ClusterCacheHandle h)
{
    return "ClusterCacheHandle("s + boost::lexical_cast<std::string>(h) + ")"s;
}

std::string
owner_tag_repr(vd::OwnerTag t)
{
    return "OwnerTag("s + boost::lexical_cast<std::string>(t) + ")"s;
}

std::string
failovercache_config_repr(const vd::FailOverCacheConfig* cfg)
{
    return "DTLConfig("s + cfg->host
            + ":"s + boost::lexical_cast<std::string>(cfg->port)
            + ","s + boost::lexical_cast<std::string>(cfg->mode)
            + ")"s;
}

// boost::python cannot deal with the cluster_cache_limit member without registering a class
// but we can use converter(s) when using properties (accessors).
boost::optional<vd::ClusterCount>
get_volume_info_cluster_cache_limit(const vfs::XMLRPCVolumeInfo* i)
{
    return i->cluster_cache_limit;
}

vd::OwnerTag
get_volume_info_owner_tag(const vfs::XMLRPCVolumeInfo* i)
{
    return i->owner_tag;
}

unsigned
mds_mdb_config_timeout_secs(const vd::MDSMetaDataBackendConfig* c)
{
    return c->timeout().count();
}

boost::optional<vfs::ObjectId>
get_client_info_object_id(const vfs::ClientInfo* ci)
{
    return ci->object_id;
}

}

TODO("AR: this is a bit of a mess - split into smaller, logical pieces");
// ... especially for converters etc that are used from different places it's unclear
// what the best place is to register them. That also includes the pickle code in
// Piccalilli.h.
BOOST_PYTHON_MODULE(storagerouterclient)
{
    youtils::Logger::disableLogging();

#include <youtils/LoggerToolCut.incl>

    bpy::scope().attr("__doc__") = "configuration and monitoring of volumedriverfs";
    bpy::scope().attr("__path__") = "storagerouterclient";

    REGISTER_EXCEPTION_TRANSLATOR(MaxRedirectsExceededException);
    REGISTER_EXCEPTION_TRANSLATOR(ClusterNotReachableException);
    REGISTER_EXCEPTION_TRANSLATOR(ObjectNotFoundException);
    REGISTER_EXCEPTION_TRANSLATOR(InvalidOperationException);
    REGISTER_EXCEPTION_TRANSLATOR(SnapshotNotFoundException);
    REGISTER_EXCEPTION_TRANSLATOR(FileExistsException);
    REGISTER_EXCEPTION_TRANSLATOR(InsufficientResourcesException);
    REGISTER_EXCEPTION_TRANSLATOR(PreviousSnapshotNotOnBackendException);
    REGISTER_EXCEPTION_TRANSLATOR(ObjectStillHasChildrenException);
    REGISTER_EXCEPTION_TRANSLATOR(SnapshotNameAlreadyExistsException);

    REGISTER_STRINGY_CONVERTER(vfs::ObjectId);
    REGISTER_STRINGY_CONVERTER(vd::VolumeId);

    REGISTER_OPTIONAL_CONVERTER(vfs::ObjectId);
    REGISTER_OPTIONAL_CONVERTER(vd::VolumeId);

    REGISTER_OPTIONAL_CONVERTER(std::string);

    REGISTER_OPTIONAL_CONVERTER(vd::ClusterCount);

    REGISTER_STRONG_ARITHMETIC_TYPEDEF_CONVERTER(vd::ClusterCount);

    REGISTER_ITERABLE_CONVERTER(std::vector<uint64_t>);

    TODO("AR: expose the new name in python as well?");
    bpy::enum_<vd::ClusterCacheBehaviour>("ReadCacheBehaviour")
        .value("CACHE_ON_WRITE", vd::ClusterCacheBehaviour::CacheOnWrite)
        .value("CACHE_ON_READ", vd::ClusterCacheBehaviour::CacheOnRead)
        .value("NO_CACHE", vd::ClusterCacheBehaviour::NoCache)
        ;

    REGISTER_OPTIONAL_CONVERTER(vd::ClusterCacheBehaviour);

    bpy::enum_<vd::ClusterCacheMode>("ReadCacheMode")
        .value("CONTENT_BASED", vd::ClusterCacheMode::ContentBased)
        .value("LOCATION_BASED", vd::ClusterCacheMode::LocationBased)
        ;

    REGISTER_OPTIONAL_CONVERTER(vd::ClusterCacheMode);

    bpy::enum_<vfs::FailOverCacheConfigMode>("DTLConfigMode")
        .value("AUTOMATIC", vfs::FailOverCacheConfigMode::Automatic)
        .value("MANUAL", vfs::FailOverCacheConfigMode::Manual)
        ;

    REGISTER_OPTIONAL_CONVERTER(vfs::FailOverCacheConfigMode);

    REGISTER_OPTIONAL_CONVERTER(uint32_t);
    REGISTER_OPTIONAL_CONVERTER(uint64_t);
    REGISTER_OPTIONAL_CONVERTER(float);

    bpy::enum_<vd::FailOverCacheMode>("DTLMode")
        .value("ASYNCHRONOUS", vd::FailOverCacheMode::Asynchronous)
        .value("SYNCHRONOUS", vd::FailOverCacheMode::Synchronous)
        ;

    REGISTER_OPTIONAL_CONVERTER(vd::FailOverCacheMode);

    bpy::class_<vd::FailOverCacheConfig>("DTLConfig",
                                         "DTL (Distributed Transaction Log) Configuration",
                                         bpy::init<std::string,
                                         uint16_t,
                                         vd::FailOverCacheMode>((bpy::args("host"),
                                                                 bpy::args("port"),
                                                                 bpy::args("mode") = vd::FailOverCacheMode::Asynchronous),
                                                                          "Instantiate a FailOverCacheConfig\n"
                                                                          "@param host, string, IP address\n"
                                                                          "@param port, uint16, port\n"
                                                                          "@param mode (Asynchronous|Synchronous)\n"))
        .def("__eq__",
             &vd::FailOverCacheConfig::operator==)
        .def("__repr__",
             &failovercache_config_repr)
#define DEF_READONLY_PROP_(name)                                \
        .def_readonly(#name, &vd::FailOverCacheConfig::name)

        DEF_READONLY_PROP_(host)
        DEF_READONLY_PROP_(port)
        DEF_READONLY_PROP_(mode)

#undef DEF_READONLY_PROP_
        ;

    REGISTER_OPTIONAL_CONVERTER(vd::FailOverCacheConfig);

    bpy::class_<vd::OwnerTag>("OwnerTag",
                              "OwnerTag",
                              bpy::init<uint64_t>())
        .def("__eq__",
             &vd::OwnerTag::operator==)
        .def("__repr__",
             &owner_tag_repr)
        .def_pickle(OwnerTagPickleSuite())
        ;

    bpy::class_<vd::ClusterCacheHandle>("ClusterCacheHandle",
                                        "ClusterCacheHandle",
                                        bpy::init<uint64_t>())
        .def("__eq__",
             &vd::ClusterCacheHandle::operator==)
        .def("__repr__",
             &cluster_cache_handle_repr)
        .def_pickle(ClusterCacheHandlePickleSuite())
        ;

    using Seconds = boost::chrono::seconds;
    using MaybeSeconds = boost::optional<Seconds>;

    REGISTER_CHRONO_DURATION_CONVERTER(Seconds);
    REGISTER_OPTIONAL_CONVERTER(Seconds);

    bpy::class_<vfs::PythonClient,
                boost::noncopyable>
        ("StorageRouterClient",
         "client for management and monitoring of a volumedriverfs cluster",
         bpy::init<const std::string&,
                   const std::vector<vfs::ClusterContact>&,
                   const MaybeSeconds&>
         ((bpy::args("vrouter_cluster_id"),
           bpy::args("cluster_contacts"),
           bpy::args("client_timeout_secs") = MaybeSeconds()),
          "Create a client interface to a volumedriverfs cluster\n"
          "@param vrouter_cluster_id: string, cluster_id \n"
          "@param cluster_contacts: [ClusterContact] contact points to the cluster\n"
          "@param client_timeout: unsigned, optional client timeout (seconds)"))
        .def("create_volume",
             &vfs::PythonClient::create_volume,
             (bpy::args("target_path"),
              bpy::args("metadata_backend_config"),
              bpy::args("volume_size"),
              bpy::args("node_id") = ""),
             "Creates a new volume under the specified path.\n"
             "@param target_path: string, volume location\n"
             "@param metadata_backend_config: MetaDataBackendConfig\n"
             "@param volume_size: string (DimensionedValue), size of the volume\n"
             "@param node_id: string, on which storagerouter to create the clone, if empty (default) the clone is created on the node receiving this call\n"
             "@raises \n"
             "      InsufficientResourcesException\n"
             "      FileExistsException\n")
        .def("unlink",
             &vfs::PythonClient::unlink,
             (bpy::args("target_path")),
             "Unlink directory entry.\n",
             "@param target_path: string, volume location\n"
             "@raises \n"
             "      RuntimeError\n")
        .def("update_metadata_backend_config",
             &vfs::PythonClient::update_metadata_backend_config,
             (bpy::args("volume_id"),
              bpy::args("metadata_backend_config")),
             "Update the volume's metadata backend config, possibly triggering a failover\n"
             "@param volume_id: string, volume identifier\n"
             "@param metadata_backend_config: MetaDataBackendConfig\n"
             "@raises\n"
             "      InvalidOperationException (config invalid / metadatastore doesn't support update)\n"
             "      PythonClientException\n")
        .def("list_volumes",
             &vfs::PythonClient::list_volumes,
             (bpy::arg("node_id") = bpy::object()),
             "List the running volumes.\n"
             "@param node_id: optional string, list volumes on a particular node)\n"
             "@returns: a list of volume IDs\n")
        .def("list_volumes_by_path",
             &vfs::PythonClient::list_volumes_by_path,
             "List the running volumes by path.")
        .def("rollback_volume",
             &vfs::PythonClient::rollback_volume,
             (bpy::args("volume_id"),
              bpy::args("snapshot_id") = ""),
             "Rollback a volume to a snapshot.\n"
             "This only succeeds if the snapshot has arrived on the backend.\n"
             "@param volume_id: string, volume identifier\n"
             "@param snapshot_id: string, snapshot identifier (optional)\n"
             "@raises \n"
             "      ObjectNotFoundException\n"
             "      SnapshotNotFoundException\n"
             "      InvalidOperationException (on template) \n"
             "      ObjectStillHasChildrenException\n")
        .def("create_clone",
             &vfs::PythonClient::create_clone,
             (bpy::args("target_path"),
              bpy::args("metadata_backend_config"),
              bpy::args("parent_volume_id"),
              bpy::args("parent_snapshot_id"),
              bpy::args("node_id") = ""),
             "Creates a clone from a snapshot of a volume.\n"
             "@param target_path: string, volume location\n"
             "@param metadata_backend_config: MetaDataBackendConfig\n"
             "@param parent_volume_id: string, parent volume identifier\n"
             "@param parent_snapshot_id: string, parent snapshot identifier\n"
             "@param node_id: string, on which storagerouter to create the clone, if empty (default) the clone is created on the node receiving this call\n"
             "@raises \n"
             "      ObjectNotFoundException\n"
             "      InvalidOperationException (parent is not a template) \n"
             "      InsufficientResourcesException\n"
             "      FileExistsException\n")
        .def("create_clone_from_template",
             &vfs::PythonClient::create_clone_from_template,
             (bpy::args("target_path"),
              bpy::args("metadata_backend_config"),
              bpy::args("parent_volume_id"),
              bpy::args("node_id") = ""),
             "Creates a clone from a template volume.\n"
             "@param target_path: string, volume location\n"
             "@param metadata_backend_config: MetaDataBackendConfig\n"
             "@param parent_volume_id: string, parent volume identifier\n"
             "@param node_id: string, on which storagerouter to create the clone, if empty (default) the clone is created on the node receiving this call\n"
             "@raises \n"
             "      ObjectNotFoundException\n"
             "      InvalidOperationException (parent is not a template) \n"
             "      InsufficientResourcesException\n"
             "      FileExistsException\n")
        .def("list_snapshots",
             &vfs::PythonClient::list_snapshots,
             (bpy::args("volume_id")),
             "List the snapshots of a volume.\n"
             "@param volume_id: string, volume identifier\n"
             "@raises \n"
             "      ObjectNotFoundException\n")
        .def("info_snapshot",
             &vfs::PythonClient::info_snapshot,
             (bpy::args("volume_id",
                        "snapshot_id")),
             "Show information about a particular snapshot of a volume.\n"
             "@param volume_id: string, volume_identifier\n"
             "@param snapshot_id: string, snapshot identifier\n"
             "@returns: SnapshotInfo object\n"
             "@raises \n"
             "      ObjectNotFoundException\n"
             "      SnapshotNotFoundException\n")
        .def("info_volume",
             &vfs::PythonClient::info_volume,
             (bpy::args("volume_id")),
             "Show information about a volume.\n"
             "@param volume_id: string, volume_identifier\n"
             "@returns: VolumeInfo object\n"
             "@raises \n"
             "      ObjectNotFoundException\n")
        .def("statistics_volume",
             &vfs::PythonClient::statistics_volume,
             (bpy::args("volume_id"),
              bpy::args("reset") = false),
             "Show detailed performance statistics about a volume.\n"
             "@param volume_id: string, volume_identifier\n"
             "@param reset: boolean, whether to reset the performance_counters\n"
             "@returns: Statistics object\n"
             "@raises \n"
             "      ObjectNotFoundException\n")
        .def("list_client_connections",
             &vfs::PythonClient::list_client_connections,
             (bpy::args("node_id")),
             "List client connections per node.\n"
             "@param node_id: string, Node ID\n"
             "@returns: ClientInfo object\n")
        .def("statistics_node",
             &vfs::PythonClient::statistics_node,
             (bpy::args("node_id"),
              bpy::args("reset") = false),
             "Show volume performance statistics aggregated per node.\n"
             "@param node_id: string, Node ID\n"
             "@param reset: boolean, whether to reset the performance_counters\n"
             "@returns: Statistics object\n")
        .def("create_snapshot",
             &vfs::PythonClient::create_snapshot,
             (bpy::args("volume_id"),
              bpy::args("snapshot_id") = "",
              bpy::args("metadata") = ""),
             "Create a snapshot.\n"
             "This only succeeds if the previous snapshot (if any) has been written to the backend.\n"
             "@param volume_id: string, volume identifier\n"
             "@param snapshot_id: string, snapshot identifier (optional)\n"
             "@param metadata: string, up to 4096 bytes of metadata (optional)\n"
             "@returns: string, snapshot identifier\n"
             "@raises \n"
             "      ObjectNotFoundException\n"
             "      InvalidOperationException (on template)\n"
             "      PreviousSnapshotNotOnBackend\n"
             )
        .def("delete_snapshot",
             &vfs::PythonClient::delete_snapshot,
             bpy::args("volume_id",
                       "snapshot_id"),
             "Delete a snapshot.\n"
             "@param volume_id: string, volume identifier\n"
             "@param snapshot_id: string, snapshot identifier\n"
             "@raises \n"
             "      ObjectNotFoundException\n"
             "      SnapshotNotFoundException\n"
             "      InvalidOperationException (on template)\n"
             "      ObjectStillHasChildrenException\n")
        .def("set_volume_as_template",
             &vfs::PythonClient::set_volume_as_template,
             (bpy::args("volume_id")),
             "Convert a volume into a template.\n"
             "Templates are read-only.\n"
             "@param volume_id: string, volume identifier\n"
             "@raises \n"
             "      ObjectNotFoundException\n"
             "      InvalidOperationException (on clone)\n")
        // .def("get_scrubbing_workunits",
        //      &vfs::PythonClient::get_scrubbing_work,
        //      (bpy::args("volume_id")),
        //      "get a list of scrubbing work units -- opaque string\n"
        //      "@param volume_id: string, volume identifier\n"
        //      "@returns: list of strings\n"
        //      "@raises \n"
        //      "      ObjectNotFoundException\n"
        //      "      InvalidOperationException (on template)\n")
        // .def("apply_scrubbing_result",
        //      &vfs::PythonClient::apply_scrubbing_result,
        //      (bpy::args("scrubbing_work_result")),
        //      "Apply a scrubbing result on the volume it's meant for\n"
        //      "@param scrubbing work result an opaque tuple returned by the scrubber\n"
        //      "@raises \n"
        //      "      ObjectNotFoundException\n"
        //      "      SnapshotNotFoundException\n"
        //      "      InvalidOperationException (on template)\n")
        .def("get_volume_id",
             &vfs::PythonClient::get_volume_id,
             (bpy::args("path")),
             "Look up the volume ID behind path (if any)\n"
             "@param path: string, path to check\n"
             "@returns: string (volume ID) or None\n")
        .def("get_object_id",
             &vfs::PythonClient::get_object_id,
             (bpy::args("path")),
             "Look up the object ID behind path (if any)\n"
             "@param path: string, path to check\n"
             "@returns: string (object ID) or None\n")
        .def("server_revision",
             &vfs::PythonClient::server_revision,
             "Get server revision information")
        .def("client_revision",
             &vfs::PythonClient::client_revision,
             "Get client revision information")
        .def("volume_potential",
             &vfs::PythonClient::volume_potential,
             (bpy::args("node_id")),
              "Check how many more volumes this node is capable of running"
              "@param node_id: string, target node"
              "@returns: int, the number of volumes the node can host")
        .def("stop_object",
             &vfs::PythonClient::stop_object,
             (bpy::args("object_id"),
              bpy::args("delete_local_data")),
             "Request that an object (volume or file) is stopped.\n"
             "\n"
             "NOTE: This does not remove the associated file - any I/O to it will lead to an error.\n"
             "@param: object_id: string, ID of the object to be stopped\n"
             "@param: delete_local_data: boolean, whether to remove local data\n"
             "@returns: eventually\n")
        .def("migrate",
             &vfs::PythonClient::migrate,
             (bpy::args("object_id",
                        "node_id",
                        "force_restart")),
             "Migrate an object (volume or file) to another node.\n"
             "@param volume_id: string, object identifier\n"
             "@param node_id: string, node to move the object to\n"
             "@param force_restart: boolean, whether to forcibly restart on the new node even if that means data loss (e.g. if the FOC is not available)\n")
        .def("restart_object",
             &vfs::PythonClient::restart_object,
             (bpy::args("object_id"),
              bpy::args("force_restart")),
             "Request that an object (volume or file) is restarted.\n"
             "@param: object_id: string, ID of the object to be restarted\n"
             "@param: force: boolean, whether to force the restart even at the expense of data loss\n"
             "@returns: eventually\n")
        .def("mark_node_offline",
             &vfs::PythonClient::mark_node_offline,
             (bpy::args("node_id")),
             "Mark a node as offline.\n"
             "This allows volumes currently registered on that node to failover to other nodes in the cluster.\n"
             "Use with caution: only mark a node as offline if it's really not running anymore.\n"
             "If the node being offlined is still running this can lead to split-brain behavior and dataloss.\n"
             "@param node_id: string, node to set mark as offline\n"
             "@raises \n"
             "      InvalidOperationException (if executed on node to be offlined)")
        .def("mark_node_online",
             &vfs::PythonClient::mark_node_online,
             (bpy::args("node_id")),
             "Mark a previously offlined node as online again.\n"
             "Only nodes marked as online can be started again.\n"
             "@param node_id: string, node to set mark as offline\n")
        .def("info_cluster",
             &vfs::PythonClient::info_cluster,
             "get an overview of configured nodes in this cluster and their status (online/offline)\n"
             "@returns: a dictionary")
        .def("set_sync_ignore",
             &vfs::PythonClient::set_sync_ignore,
             (bpy::args("volume_id"),
              bpy::args("number_of_syncs_to_ignore"),
              bpy::args("maximum_time_to_ignore_syncs_in_seconds")),
             "Set the sync ignore settings.\n"
             "@param volume_id: string, volume identifier\n"
             "@param number_of_syncs_to_ignore: uint64, the number of syncs to ignore\n"
             "@param maximum_time_to_ignore_syncs_in_seconds: uint64, maximum time to ignore syncs in seconds\n")
        .def("get_sync_ignore",
             &vfs::PythonClient::get_sync_ignore,
             (bpy::args("volume_id")),
             "get the sync ignore settings.\n"
             "@param volume_id: string, volume identifier\n"
             "@returns a dict with the relevant settings")
         .def("set_sco_multiplier",
              &vfs::PythonClient::set_sco_multiplier,
              (bpy::args("volume_id"),
               bpy::args("sco_multiplier")),
              "Set the number of clusters in a SCO.\n"
              "@param volume_id: string, volume identifier\n"
              "@param sco_multiplier: uint32, the number of clusters in a SCO\n"
              "@raises \n"
              "      InvalidOperationException (if SCO size would become invalid)")
         .def("get_sco_multiplier",
              &vfs::PythonClient::get_sco_multiplier,
              (bpy::args("volume_id")),
              "Get the number of clusters in a SCO.\n"
              "@param volume_id: string, volume identifier\n"
              "@returns a uint32")
         .def("set_tlog_multiplier",
              &vfs::PythonClient::set_tlog_multiplier,
              (bpy::args("volume_id"),
               bpy::args("tlog_multiplier")),
              "Set the number of SCO's in a TLOG.\n"
              "@param volume_id: string, volume identifier\n"
              "@param tlog_multiplier: None (= volumedriver global value is used) or uint32, the number of SCO's in a TLOG\n"
              "@raises \n"
              "      InvalidOperationException (if TLOG size would become invalid)")
         .def("get_tlog_multiplier",
              &vfs::PythonClient::get_tlog_multiplier,
              (bpy::args("volume_id")),
              "Get the number of SCO's in a TLOG.\n"
              "@param volume_id: string, volume identifier\n"
              "@returns a (optional) uint32")
         .def("set_sco_cache_max_non_disposable_factor",
              &vfs::PythonClient::set_sco_cache_max_non_disposable_factor,
              (bpy::args("volume_id"),
               bpy::args("max_non_disposable_factor")),
              "Set the factor of non-disposable data.\n"
              "@param volume_id: string, volume identifier\n"
              "@param max_non_disposable_factor: None (= volumedriver global value is used) or float, the factor of non-disposable data\n")
         .def("get_sco_cache_max_non_disposable_factor",
              &vfs::PythonClient::get_sco_cache_max_non_disposable_factor,
              (bpy::args("volume_id")),
              "Get the factor of non-disposable data.\n"
              "@param volume_id: string, volume identifier\n"
              "@returns a (optional) float")
          .def("get_dtl_config_mode",
               &vfs::PythonClient::get_failover_cache_config_mode,
               (bpy::args("volume_id")),
               "get a node's DTL configuration mode\n"
               "@param volume_id: string, volume identifier\n"
               "@returns the DTL configuration mode of the node of the volume: Automatic | Manual\n")
          .def("get_dtl_config",
               &vfs::PythonClient::get_failover_cache_config,
               (bpy::args("volume_id")),
               "get a volume's DTL configuration\n"
               "@param volume_id: string, volume identifier\n"
               "@returns the DTL configuration of the volume\n")
          .def("set_manual_dtl_config",
               &vfs::PythonClient::set_manual_failover_cache_config,
               (bpy::args("node_id"),
                bpy::args("config")),
               "set a volume's DTL configuration\n"
               "@param volume_id: string, volume identifier\n"
               "@param config: a DTLConfig, or 'None' for no DTL")
          .def("set_automatic_dtl_config",
               &vfs::PythonClient::set_automatic_failover_cache_config,
               (bpy::args("node_id")),
               "set a volume's DTL configuration to Automatic\n"
               "@param volume_id: string, volume identifier\n")
        .def("get_readcache_behaviour",
             &vfs::PythonClient::get_cluster_cache_behaviour,
             (bpy::args("volume_id")),
             "get a volume's readcache behaviour\n"
             "@param volume_id: string, volume identifier\n"
             "@returns None (= volumedriver global value is used) or a ReadCacheBehaviour value\n")
        .def("set_readcache_behaviour",
             &vfs::PythonClient::set_cluster_cache_behaviour,
             (bpy::args("volume_id"),
              bpy::args("behaviour")),
             "set a volume's readcache behaviour\n"
             "@param volume_id: string, volume identifier\n"
             "@param behaviour: None (= volumedriver global value is used) or a ReadCacheBehaviour value\n"
             "@returns: nothing, eventually\n")
        .def("get_readcache_mode",
             &vfs::PythonClient::get_cluster_cache_mode,
             (bpy::args("volume_id")),
             "get a volume's readcache mode\n"
             "@param volume_id: string, volume identifier\n"
             "@returns None (= volumedriver global value is used) or a ReadCacheMode value\n")
        .def("set_readcache_mode",
             &vfs::PythonClient::set_cluster_cache_mode,
             (bpy::args("volume_id"),
              bpy::args("mode")),
             "set a volume's readcache mode\n"
             "@param volume_id: string, volume identifier\n"
             "@param mode: None (= volumedriver global value is used) or a ReadCacheMode value\n"
             "@returns: nothing, eventually\n")
        .def("get_readcache_limit",
             &vfs::PythonClient::get_cluster_cache_limit,
             (bpy::args("volume_id")),
             "get a volume's readcache limit (when in LocationBased mode)\n"
             "@param volume_id: string, volume identifier\n"
             "@returns None (= no limit) or the maximum number of clusters the volume can cache\n")
        .def("set_readcache_limit",
             &vfs::PythonClient::set_cluster_cache_limit,
             (bpy::args("volume_id"),
              bpy::args("limit")),
             "set a volume's readcache limit (when in LocationBased mode)\n"
             "@param volume_id: string, volume identifier\n"
             "@param limit: None (= no limit) or an integer specifying the maximum number of clusters\n"
             "@returns: nothing, eventually\n")
        .def("update_cluster_node_configs",
             &vfs::PythonClient::update_cluster_node_configs,
             (bpy::args("node_id") = ""),
             "Re-read the cluster configuration from the ClusterRegistry.\n"
             "@param node_id: string, on which storagerouter to re-read the configuration\n")
        .def("make_locked_client",
             &vfs::PythonClient::make_locked_client,
             (bpy::args("nspace"),
              bpy::args("update_interval_secs") = 3,
              bpy::args("grace_period_secs") = 7),
             "Create a context manager for a locked namespace.\n"
             "@param nspace: string, namespace.\n"
             "@param update_interval_secs: unsigned, update interval\n"
             "@param grace_period_secs: unsigned, grace period\n"
             "@returns: LockedClient context manager\n")
        .def("schedule_backend_sync",
             &vfs::PythonClient::schedule_backend_sync,
             (bpy::args("volume_id")),
             "Schedule a backend sync of the given Volume\n"
             "@param volume_id: string, volume identifier\n"
             "@returns: string, name of the TLog covering the data written out\n")
        .def("is_volume_synced_up_to_tlog",
             &vfs::PythonClient::is_volume_synced_up_to_tlog,
             (bpy::args("volume_id"),
              bpy::args("tlog_name")),
             "Check whether a volume is synced to the backend up to a given TLog\n"
             "@param volume_id: string, volume identifier\n"
             "@param tlog_name: string, TLog name\n"
             "@returns: boolean\n")
        .def("is_volume_synced_up_to_snapshot",
             &vfs::PythonClient::is_volume_synced_up_to_snapshot,
             (bpy::args("volume_id"),
              bpy::args("snapshot_name")),
             "Check whether a volume is synced to the backend up to a given snapshot\n"
             "@param volume_id: string, volume identifier\n"
             "@param snapshot_name: string, snapshot name\n"
             "@returns: boolean\n")
        .def("set_metadata_cache_capacity",
             &vfs::PythonClient::set_metadata_cache_capacity,
             (bpy::args("volume_id"),
              bpy::args("num_pages")),
             "Set the volume's metadata cache capacity\n"
             "@param volume_id: string, volume identifier\n"
             "@param num_pages, unsigned, number of metadata pages\n")
        .def("get_metadata_cache_capacity",
             &vfs::PythonClient::get_metadata_cache_capacity,
             (bpy::args("volume_id")),
             "Get the volume's metadata cache capacity\n"
             "@param volume_id: string, volume identifier\n"
             "@returns num_pages, unsigned, number of metadata pages or None\n")
        .def("client_timeout_secs",
             &vfs::PythonClient::timeout,
             bpy::return_value_policy<bpy::copy_const_reference>(),
             "Get the client timeout (if any) in seconds\n"
             "@returns: unsigned, client timeout in seconds, or None\n")
        ;

    vfspy::LocalClient::registerize();
    vfspy::ArakoonClient::registerize();

    REGISTER_STRINGY_CONVERTER(vfs::ClusterId);
    REGISTER_STRINGY_CONVERTER(vfs::NodeId);

    REGISTER_STRONG_ARITHMETIC_TYPEDEF_CONVERTER(vfs::MessagePort);
    REGISTER_STRONG_ARITHMETIC_TYPEDEF_CONVERTER(vfs::XmlRpcPort);
    REGISTER_STRONG_ARITHMETIC_TYPEDEF_CONVERTER(vfs::FailoverCachePort);

    bpy::class_<vfs::ClusterNodeConfig>("ClusterNodeConfig",
                                        "configuration of a single volumedriverfs cluster node",
                                        bpy::init<const vfs::NodeId&,
                                                  const std::string&,
                                                  vfs::MessagePort,
                                                  vfs::XmlRpcPort,
                                                  vfs::FailoverCachePort>((bpy::args("vrouter_id"),
                                                                           bpy::args("host"),
                                                                           bpy::args("message_port"),
                                                                           bpy::args("xmlrpc_port"),
                                                                           bpy::args("failovercache_port")),
                                                                          "Create a cluster node configuration\n"
                                                                          "@param vrouter_id: string, node ID\n"
                                                                          "@param host: string, hostname or IP address\n"
                                                                          "@param message_port: uint16_t, TCP port used for communication between nodes\n"
                                                                          "@param xmlrpc_port: uint16_t, TCP port used for XMLRPC based management\n"
                                                                          "@param failovercache_port: uint16_t, TCP port of the FailoverCache for this node\n"))
        .def("__str__", &vfs::ClusterNodeConfig::str)
        .def("__repr__", &vfs::ClusterNodeConfig::str)
        .def("__eq__", &vfs::ClusterNodeConfig::operator==)

#define DEF_READONLY_PROP_(name)                                \
        .def_readonly(#name, &vfs::ClusterNodeConfig::name)

        DEF_READONLY_PROP_(vrouter_id)
        DEF_READONLY_PROP_(host)
        DEF_READONLY_PROP_(message_port)
        DEF_READONLY_PROP_(xmlrpc_port)
        DEF_READONLY_PROP_(failovercache_port)

#undef DEF_READONLY_PROP_
        ;

    REGISTER_ITERABLE_CONVERTER(std::vector<vfs::ClusterNodeConfig>);

    bpy::class_<vfs::ClientInfo>("ClientInfo",
                                 "Client connection information",
                                 bpy::init<const boost::optional<vfs::ObjectId>&,
                                           const std::string&,
                                           const uint16_t>((bpy::args("object_id"),
                                                            bpy::args("ip"),
                                                            bpy::args("port")),
                                                           "Create a client connection information object\n"
                                                           "@param object_id: string, Object ID\n"
                                                           "@param ip: string, IP address\n"
                                                           "@param port: uint16_t, TCP/IP or RDMA client port\n"))
        .def("__str__", &vfs::ClientInfo::str)
        .def("__repr__", &vfs::ClientInfo::str)

#define DEF_READONLY_PROP_(name)                                \
        .def_readonly(#name, &vfs::ClientInfo::name)

        DEF_READONLY_PROP_(object_id)
        DEF_READONLY_PROP_(ip)
        DEF_READONLY_PROP_(port)
        .add_property("object_id",
                      &get_client_info_object_id)

#undef DEF_READONLY_PROP_
        ;
    REGISTER_ITERABLE_CONVERTER(std::vector<vfs::ClientInfo>);

    bpy::class_<vfs::ClusterRegistry,
                boost::noncopyable>("ClusterRegistry",
                                    "volumedriverfs cluster registry access",
                                    bpy::init<const vfs::ClusterId&,
                                    const ara::ClusterID&,
                                    const std::vector<ara::ArakoonNodeConfig>&,
                                    const unsigned>((bpy::args("vfs_cluster_id"),
                                                     bpy::args("ara_cluster_id"),
                                                     bpy::args("ara_node_configs"),
                                                     bpy::args("ara_timeout_secs") = 60),
                                                    "Create a ClusterRegistry instance\n"
                                                    "@param vfs_cluster_id: string, volumedriverfs cluster ID\n"
                                                    "@param ara_cluster_id: string, Arakoon cluster ID\n"
                                                    "@param ara_node_configs: list of ArakoonNodeConfigs\n"
                                                    "@param ara_timeout_secs: unsigned, timeout (in seconds) for Arakoon requests\n"))
        .def("set_node_configs",
             &vfs::ClusterRegistry::set_node_configs,
             (bpy::args("cluster_node_configs")),
             "Store the configurations of this cluster's nodes in the registry.\n"
             "@param cluster_node_configs: list of ClusterNodeConfig entries\n")
        .def("get_node_configs",
             &vfs::ClusterRegistry::get_node_configs,
             "Retrieve the list of ClusterNodeConfig entries for this cluster from the registry.\n")
        .def("erase_node_configs",
             &vfs::ClusterRegistry::erase_node_configs,
             "Erase the cluster node configurations for this cluster from the registry\n")
        ;

    bpy::enum_<vfs::ObjectType>("ObjectType")
        .value("BASE", vfs::ObjectType::Volume)
        .value("TEMPLATE", vfs::ObjectType::Template)
        .value("FILE", vfs::ObjectType::File)
        ;

    bpy::enum_<vfs::ClusterNodeStatus::State>("ClusterNodeState")
        .value("OFFLINE", vfs::ClusterNodeStatus::State::Offline)
        .value("ONLINE", vfs::ClusterNodeStatus::State::Online);

#define DEF_READONLY_PROP_(name)                        \
    .def_readonly(#name, &vfs::XMLRPCVolumeInfo::name)

    bpy::class_<vfs::XMLRPCVolumeInfo>("VolumeInfo")
        .def("__str__", &vfs::XMLRPCVolumeInfo::str)
        .def("__repr__", &vfs::XMLRPCVolumeInfo::str)
        .def("__eq__", &vfs::XMLRPCVolumeInfo::operator==)
        DEF_READONLY_PROP_(volume_id)
        .def_readonly("namespace", &vfs::XMLRPCVolumeInfo::_namespace_)
        DEF_READONLY_PROP_(parent_namespace)
        DEF_READONLY_PROP_(parent_snapshot_id)
        DEF_READONLY_PROP_(volume_size)
        DEF_READONLY_PROP_(lba_size)
        DEF_READONLY_PROP_(lba_count)
        DEF_READONLY_PROP_(cluster_multiplier)
        DEF_READONLY_PROP_(sco_multiplier)
        DEF_READONLY_PROP_(failover_mode)
        DEF_READONLY_PROP_(failover_ip)
        DEF_READONLY_PROP_(failover_port)
        DEF_READONLY_PROP_(halted)
        DEF_READONLY_PROP_(footprint)
        DEF_READONLY_PROP_(stored)
        DEF_READONLY_PROP_(object_type)
        DEF_READONLY_PROP_(parent_volume_id)
        DEF_READONLY_PROP_(vrouter_id)
        DEF_READONLY_PROP_(metadata_backend_config)
        DEF_READONLY_PROP_(cluster_cache_handle)
        .add_property("owner_tag",
                      &get_volume_info_owner_tag)
        .add_property("cluster_cache_limit",
                      &get_volume_info_cluster_cache_limit)
        .def_pickle(XMLRPCVolumeInfoPickleSuite())
        ;
#undef DEF_READONLY_PROP_

    bpy::class_<vd::PerformanceCounter<uint64_t>>("PerformanceCounterU64")
        .def("__eq__",
             &vd::PerformanceCounter<uint64_t>::operator==)
        .def("__repr__",
             &repr<vd::PerformanceCounter<uint64_t>>)
        .def("events",
             &vd::PerformanceCounter<uint64_t>::events)
        .def("min",
             &vd::PerformanceCounter<uint64_t>::min)
        .def("max",
             &vd::PerformanceCounter<uint64_t>::max)
        .def("sum",
             &vd::PerformanceCounter<uint64_t>::sum)
        .def("sum_of_squares",
             &vd::PerformanceCounter<uint64_t>::sum_of_squares)
        .def_pickle(PerformanceCounterU64PickleSuite())
        ;

#define DEF_READONLY_PROP_(name)                        \
    .def_readonly(#name, &vd::PerformanceCounters::name)

    bpy::class_<vd::PerformanceCounters>("PerformanceCounters")
        .def("__eq__",
             &vd::PerformanceCounters::operator==)
        .def("__repr__",
             &repr<vd::PerformanceCounters>)
        DEF_READONLY_PROP_(write_request_size)
        DEF_READONLY_PROP_(write_request_usecs)
        DEF_READONLY_PROP_(unaligned_write_request_size)
        DEF_READONLY_PROP_(backend_write_request_size)
        DEF_READONLY_PROP_(backend_write_request_usecs)
        DEF_READONLY_PROP_(read_request_size)
        DEF_READONLY_PROP_(read_request_usecs)
        DEF_READONLY_PROP_(unaligned_read_request_size)
        DEF_READONLY_PROP_(backend_read_request_size)
        DEF_READONLY_PROP_(backend_read_request_usecs)
        DEF_READONLY_PROP_(sync_request_usecs)
        .def_pickle(PerformanceCountersPickleSuite())
        ;
#undef DEF_READONLY_PROP_

#define DEF_READONLY_PROP_(name)                        \
    .def_readonly(#name, &vfs::XMLRPCStatistics::name)

    bpy::class_<vfs::XMLRPCStatistics>("Statistics")
        .def("__str__", &vfs::XMLRPCStatistics::str)
        .def("__repr__", &vfs::XMLRPCStatistics::str)
        .def("__eq__", &vfs::XMLRPCStatistics::operator==)
        DEF_READONLY_PROP_(sco_cache_hits)
        DEF_READONLY_PROP_(sco_cache_misses)
        DEF_READONLY_PROP_(cluster_cache_hits)
        DEF_READONLY_PROP_(cluster_cache_misses)
        DEF_READONLY_PROP_(metadata_store_hits)
        DEF_READONLY_PROP_(metadata_store_misses)
        DEF_READONLY_PROP_(stored)
        DEF_READONLY_PROP_(performance_counters)
        .def_pickle(XMLRPCStatisticsPickleSuite())
        ;
#undef DEF_READONLY_PROP_

#define DEF_READONLY_PROP_(name)                                \
    .def_readonly(#name, &vfs::XMLRPCSnapshotInfo::name)

    bpy::class_<vfs::XMLRPCSnapshotInfo>("SnapshotInfo")
        .def("__str__", &vfs::XMLRPCSnapshotInfo::str)
        .def("__repr__", &vfs::XMLRPCSnapshotInfo::str)
        .def("__eq__", &vfs::XMLRPCSnapshotInfo::operator==)
        DEF_READONLY_PROP_(snapshot_id)
        DEF_READONLY_PROP_(timestamp)
        DEF_READONLY_PROP_(uuid)
        DEF_READONLY_PROP_(stored)
        DEF_READONLY_PROP_(metadata)
        DEF_READONLY_PROP_(in_backend)
        .def_pickle(XMLRPCSnapshotInfoPickleSuite())
        ;
#undef DEF_READONLY_PROP_

     bpy::class_<vfs::ClusterContact>("ClusterContact",
                                      "simple object to hold an ip and port to a vrouter node",
                                      bpy::init<const std::string&,
                                                const uint16_t>(bpy::args("host",
                                                                          "port")))
            .def_readonly("host", &vfs::ClusterContact::host)
            .def_readonly("port", &vfs::ClusterContact::port);

    bpy::to_python_converter<yt::UpdateReport, UpdateReportConverter>();

    typedef std::map<vfs::NodeId, vfs::ClusterNodeStatus::State> NodeStatusMap;
    bpy::to_python_converter<NodeStatusMap, MapToDictConverter<NodeStatusMap>>();

    REGISTER_ITERABLE_CONVERTER(std::vector<vfs::ClusterContact>);

    vfspy::PythonTestHelpers::registerize();

    bpy::class_<vd::MDSNodeConfig>("MDSNodeConfig",
                                   "MetaDataServer node configuration",
                                   bpy::init<const std::string&,
                                             uint16_t>((bpy::args("address"),
                                                        bpy::args("port")),
                                                       "Create a MetaDataServer node configuration\n"
                                                       "@param address: string, address of the server\n"
                                                       "@param port, uint16_t, TCP port of the server\n"))
        .def("address",
             &vd::MDSNodeConfig::address,
             bpy::return_value_policy<bpy::copy_const_reference>())
        .def("port",
             &vd::MDSNodeConfig::port)
        ;

    REGISTER_ITERABLE_CONVERTER(std::vector<vd::MDSNodeConfig>);

    bpy::class_<vd::MetaDataBackendConfig,
                boost::noncopyable>("MetaDataBackendConfig",
                                    "Abstract base class for MetaDataBackends",
                                    bpy::no_init);

    // AR: boost::python does not support move semantics
    // (ATM? https://svn.boost.org/trac/boost/ticket/7711), so we use a shared_ptr
    // and convert (by creating a copy) in PythonClient.
    bpy::register_ptr_to_python<boost::shared_ptr<vd::MetaDataBackendConfig>>();

    bpy::class_<vd::TCBTMetaDataBackendConfig,
                bpy::bases<vd::MetaDataBackendConfig>,
                boost::shared_ptr<vd::TCBTMetaDataBackendConfig>>
        ("TCBTMetaDataBackendConfig",
         "TokyoCabinet B+-Tree metadata backend configuration")
        ;

    bpy::class_<vd::RocksDBMetaDataBackendConfig,
                bpy::bases<vd::MetaDataBackendConfig>,
                boost::shared_ptr<vd::RocksDBMetaDataBackendConfig>>
        ("RocksDBMetaDataBackendConfig",
         "RocksDB metadata backend configuration")
        ;

    bpy::class_<vd::ArakoonMetaDataBackendConfig,
                bpy::bases<vd::MetaDataBackendConfig>,
                boost::shared_ptr<vd::ArakoonMetaDataBackendConfig>>
        ("ArakoonMetaDataBackendConfig",
         "Arakoon metadata backend configuration",
         bpy::init<const ara::ClusterID&,
                   const std::vector<ara::ArakoonNodeConfig>&>((bpy::args("ara_cluster_id"),
                                                                bpy::args("ara_node_configs")),
                                                               "Create an ArakoonMetaDataBackendConfig\n"
                                                               "@param ara_cluster_id: string, Arakoon cluster ID\n"
                                                               "@param ara_node_configs: list of ArakoonNodeConfigs\n"))
        .def("cluster_id",
             &vd::ArakoonMetaDataBackendConfig::cluster_id,
             bpy::return_value_policy<bpy::copy_const_reference>())
        .def("node_configs",
             &vd::ArakoonMetaDataBackendConfig::node_configs,
             bpy::return_value_policy<bpy::copy_const_reference>())
        ;

    bpy::enum_<vd::ApplyRelocationsToSlaves>("ApplyRelocationsToSlaves",
                                             "BooleanEnum indicating whether scrub results should be applied to slaves or not")
        .value("F", vd::ApplyRelocationsToSlaves::F)
        .value("T", vd::ApplyRelocationsToSlaves::T)
        ;

    bpy::class_<vd::MDSMetaDataBackendConfig,
                bpy::bases<vd::MetaDataBackendConfig>,
                boost::shared_ptr<vd::MDSMetaDataBackendConfig>>
        ("MDSMetaDataBackendConfig",
         "MDS metadata backend configuration",
         bpy::init<const std::vector<vd::MDSNodeConfig>&,
                   vd::ApplyRelocationsToSlaves,
                   unsigned>((bpy::args("mds_node_configs"),
                              bpy::args("apply_relocations_to_slaves") = vd::ApplyRelocationsToSlaves::T,
                              bpy::args("timeout_secs") = vd::MDSMetaDataBackendConfig::default_timeout_secs_),
                             "Create an MDSMetaDataBackendConfig\n"
                             "@param mds_node_configs: list of MDSNodeConfigs\n"
                             "@param apply_relocations_to_slaves: ApplyRelocationsToSlaves boolean enum\n"
                             "@param timeout_secs: unsigned, timeout for remote MDS calls in seconds"))
        .def("node_configs",
             &vd::MDSMetaDataBackendConfig::node_configs,
             bpy::return_value_policy<bpy::copy_const_reference>())
        .def("apply_relocations_to_slaves",
             &vd::MDSMetaDataBackendConfig::apply_relocations_to_slaves)
        .def("timeout_secs",
             &mds_mdb_config_timeout_secs)
        ;

    REGISTER_ITERABLE_CONVERTER(std::vector<std::string>);
    REGISTER_ITERABLE_CONVERTER(std::list<std::string>);

    vfspy::LockedClient::registerize();
    vfspy::MDSClient::registerize();
    youtils::python::BuildInfo::registerize();
    scrubbing::python::Scrubber::registerize();

    vfspy::ObjectRegistryClient::registerize();

    export_debug_module();
}

#undef REGISTER_EXCEPTION_TRANSLATOR
#undef DEFINE_EXCEPTION_TRANSLATOR
#undef PYTHON_EXCEPTION_TYPE
