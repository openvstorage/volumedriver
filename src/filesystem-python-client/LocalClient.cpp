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

#include "IterableConverter.h"
#include "LocalClient.h"
#include "OptionalConverter.h"
#include "PairConverter.h"
#include "Piccalilli.h"
#include "StrongArithmeticTypedefConverter.h"

#include <boost/python/class.hpp>
#include <boost/python/enum.hpp>

#include <youtils/Logger.h>

#include <volumedriver/ClusterCount.h>

#include <filesystem/LocalPythonClient.h>
#include <filesystem/XMLRPCStructs.h>

namespace bpy = boost::python;
namespace vd = volumedriver;
namespace vfs = volumedriverfs;
namespace yt = youtils;

namespace volumedriverfs
{

namespace python
{

namespace
{

// boost::python cannot deal with the max_entries member without registering a class
// but we can use converter(s) when using properties (accessors).
boost::optional<uint64_t>
get_cluster_cache_handle_max_entries(const vfs::XMLRPCClusterCacheHandleInfo* i)
{
    return i->max_entries;
}

std::vector<uint64_t>
get_cluster_cache_handle_map_stats(const vfs::XMLRPCClusterCacheHandleInfo* i)
{
    return i->map_stats;
}

}

void
LocalClient::registerize()
{
    // Severity comes from youtils/LoggerToolCut.incl
    REGISTER_PAIR_CONVERTER(std::string, yt::Severity);

    // the typedef is necessary here, otherwise the ',' will lead to mayhem with the
    // REGISTER_ITERABLE_CONVERTER macro. Sigh.
    typedef std::pair<std::string, yt::Severity> LogFilter;
    REGISTER_ITERABLE_CONVERTER(std::vector<LogFilter>);

    REGISTER_ITERABLE_CONVERTER(std::vector<vd::ClusterCacheHandle>);

#define DEF_READONLY_PROP_(name)                        \
    .def_readonly(#name, &vfs::XMLRPCClusterCacheHandleInfo::name)

    bpy::class_<vfs::XMLRPCClusterCacheHandleInfo>("ClusterCacheHandleInfo")
        .def("__str__",
             &vfs::XMLRPCClusterCacheHandleInfo::str)
        .def("__repr__",
             &vfs::XMLRPCClusterCacheHandleInfo::str)
        .def("__eq__",
             &vfs::XMLRPCClusterCacheHandleInfo::operator==)
        DEF_READONLY_PROP_(cluster_cache_handle)
        DEF_READONLY_PROP_(entries)
        .add_property("max_entries",
                      &get_cluster_cache_handle_max_entries)
        .add_property("map_stats",
                      &get_cluster_cache_handle_map_stats)
        .def_pickle(XMLRPCClusterCacheHandleInfoPickleSuite())
        ;

#undef DEF_READONLY_PROP_

    using MaybeSeconds = boost::optional<boost::chrono::seconds>;

    bpy::class_<vfs::LocalPythonClient,
                boost::noncopyable,
                bpy::bases<vfs::PythonClient>>
        ("LocalStorageRouterClient",
         "maintenance client for management and monitoring of a specific volumedriverfs cluster node",
         bpy::init<const std::string&,
                   const MaybeSeconds&>
         ((bpy::args("config_location"),
           bpy::args("client_timeout") = MaybeSeconds()),
          "Create a maintenance client interface to a volumedriverfs cluster\n"
          "@param config_location: string, URI of the instance's configuration\n"
          "@param client_timeout: unsigned, optional client timeout (seconds)"))
        .def("destroy_filesystem",
             &vfs::LocalPythonClient::destroy,
             (bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Request that the filesystem is destroyed.\n"
             "All data and metadata in Arakoon and the backend will be removed.\n"
             "Local data (caches) will not be removed.\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n")
        .def("get_running_configuration",
             &vfs::LocalPythonClient::get_running_configuration,
             (bpy::args("report_defaults") = false,
              bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Return a string containing the current configuration (JSON format)\n"
             "@param: report_defaults: bool, report default values (default: False)\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n"
             "@returns: string, configuration data (JSON)\n")
        .def("update_configuration",
             &vfs::LocalPythonClient::update_configuration,
             (bpy::args("config"),
              bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Request volumedriverfs to update its configuration from the given file\n"
             "@param: config: string, path to the config file (JSON) / etcd URL\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n"
             "@returns: a list of successfully applied updates - throws in case of error\n")
        .def("get_general_logging_level",
             &vfs::LocalPythonClient::get_general_logging_level,
             (bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Get the currently active global loglevel\n"
             "Note that logging needs to be enabled, otherwise this call will throw\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n"
             "@returns: Severity enum value\n")
        .def("set_general_logging_level",
             &vfs::LocalPythonClient::set_general_logging_level,
             (bpy::args("severity"),
              bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Set the global loglevel\n"
             "Note that logging needs to be enabled, otherwise this call will throw\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n"
             "@param severity: Severity enum value\n")
        .def("list_logging_filters",
             &vfs::LocalPythonClient::get_logging_filters,
             (bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Get a list of all active logging filters\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n"
             "@returns: a list of (string, Severity) pairs\n")
        .def("add_logging_filter",
             &vfs::LocalPythonClient::add_logging_filter,
             (bpy::args("filter_match"),
              bpy::args("severity"),
              bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Add a logging filter\n"
             "@param filter_match: string\n"
             "@param severity: Severity enum value\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n")
        .def("remove_logging_filter",
             &vfs::LocalPythonClient::remove_logging_filter,
             (bpy::args("filter_match"),
              bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Remove a specific logging filter\n"
             "@param filter_match: string"
             "@param req_timeout_secs: optional timeout in seconds for this request\n")
        .def("remove_logging_filters",
             &vfs::LocalPythonClient::remove_logging_filters,
             (bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Remove *all* logging filters\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n")
        .def("malloc_info",
             &vfs::LocalPythonClient::malloc_info,
             (bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Retrieve allocator info from the filesystem instance for debugging purposes\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n"
             "@returns: a string containing the allocator's malloc_info\n")
        .def("list_cluster_cache_handles",
             &vfs::LocalPythonClient::list_cluster_cache_handles,
             (bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Get a list of ClusterCacheHandles\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n"
             "@returns: a list containing ClusterCacheHandles\n")
        .def("get_cluster_cache_handle_info",
             &vfs::LocalPythonClient::get_cluster_cache_handle_info,
             (bpy::args("handle"),
              bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Obtain detailed information about a ClusterCacheHandle\n"
             "@param handle, ClusterCacheHandle\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n"
             "@returns: a ClusterCacheHandleInfo object\n")
        .def("remove_cluster_cache_handle",
             &vfs::LocalPythonClient::remove_cluster_cache_handle,
             (bpy::args("handle"),
              bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Remove a ClusterCacheHandle (and its entries) from the ClusterCache\n"
             "@param handle, ClusterCacheHandle\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n"
             "@returns: a ClusterCacheHandleInfo object\n")
        .def("_remove_namespace_from_sco_cache",
             &vfs::LocalPythonClient::remove_namespace_from_sco_cache,
             (bpy::args("nspace"),
              bpy::args("req_timeout_secs") = MaybeSeconds()),
             "Remove a namespace (and its SCOs) from the SCO cache\n"
             "@param nspace, string, namespace name\n"
             "@param req_timeout_secs: optional timeout in seconds for this request\n")
        ;
}

}

}
