// Copyright 2015 Open vStorage NV
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

    bpy::class_<vfs::LocalPythonClient,
                bpy::bases<vfs::PythonClient>>("LocalStorageRouterClient",
                                               "maintenance client for management and monitoring of a specific volumedriverfs cluster node",
                    bpy::init<const std::string&>
                    (bpy::args("config_file"),
                     "Create a maintenance client interface to a volumedriverfs cluster\n"
                     "@param config_file: string, path to the cluster config file\n"))
        .def("destroy_filesystem",
             &vfs::LocalPythonClient::destroy,
             "Request that the filesystem is destroyed.\n"
             "All data and metadata in Arakoon and the backend will be removed.\n"
             "Local data (caches) will not be removed.\n")
        .def("stop_object",
             &vfs::LocalPythonClient::stop_object,
             (bpy::args("object_id"),
              bpy::args("delete_local_data")),
             "Request that an object (volume or file) is stopped.\n"
             "\n"
             "NOTE: This does not remove the associated file - any I/O to it will lead to an error.\n"
             "@param: object_id: string, ID of the object to be stopped\n"
             "@param: delete_local_data: boolean, whether to remove local data\n"
             "@returns: eventually\n")
        .def("restart_object",
             &vfs::LocalPythonClient::restart_object,
             (bpy::args("object_id"),
              bpy::args("force_restart")),
             "Request that an object (volume or file) is restarted.\n"
             "@param: object_id: string, ID of the object to be restarted\n"
             "@param: force: boolean, whether to force the restart even at the expense of data loss\n"
             "@returns: eventually\n")

        .def("get_running_configuration",
             &vfs::LocalPythonClient::get_running_configuration,
             (bpy::args("report_defaults") = false),
             "Return a string containing the current configuration (JSON format)\n"
             "@param: report_defaults: bool, report default values (default: False)\n"
             "@returns: string, configuration data (JSON)\n")
        .def("update_configuration",
             &vfs::LocalPythonClient::update_configuration,
             (bpy::args("config_file_path")),
             "Request volumedriverfs to update its configuration from the given file\n"
             "@param: config_file_path: string, path to the config file (JSON)\n"
             "@returns: a list of successfully applied updates - throws in case of error\n")
        .def("get_general_logging_level",
             &vfs::LocalPythonClient::get_general_logging_level,
             "Get the currently active global loglevel\n"
             "Note that logging needs to be enabled, otherwise this call will throw\n"
             "@returns: Severity enum value\n")
        .def("set_general_logging_level",
             &vfs::LocalPythonClient::set_general_logging_level,
             (bpy::args("severity")),
             "Set the global loglevel\n"
             "Note that logging needs to be enabled, otherwise this call will throw\n"
             "@param severity: Severity enum value\n")
        .def("list_logging_filters",
             &vfs::LocalPythonClient::get_logging_filters,
             "Get a list of all active logging filters\n"
             "@returns: a list of (string, Severity) pairs\n")
        .def("add_logging_filter",
             &vfs::LocalPythonClient::add_logging_filter,
             (bpy::args("filter_match"),
              bpy::args("severity")),
             "Add a logging filter\n"
             "@param filter_match: string\n"
             "@param severity: Severity enum value\n")
        .def("remove_logging_filter",
             &vfs::LocalPythonClient::remove_logging_filter,
             (bpy::args("filter_match")),
             "Remove a specific logging filter\n"
             "@param filter_match: string")
        .def("remove_logging_filters",
             &vfs::LocalPythonClient::remove_logging_filters,
             "Remove *all* logging filters\n")
        .def("malloc_info",
             &vfs::LocalPythonClient::malloc_info,
             "Retrieve allocator info from the filesystem instance for debugging purposes\n"
             "@returns: a string containing the allocator's malloc_info\n")
        .def("list_cluster_cache_handles",
             &vfs::LocalPythonClient::list_cluster_cache_handles,
             "Get a list of ClusterCacheHandles\n"
             "@returns: a list containing ClusterCacheHandles\n")
        .def("get_cluster_cache_handle_info",
             &vfs::LocalPythonClient::get_cluster_cache_handle_info,
             (bpy::args("handle")),
             "Obtain detailed information about a ClusterCacheHandle\n"
             "@param handle, ClusterCacheHandle\n"
             "@returns: a ClusterCacheHandleInfo object\n")
        .def("remove_cluster_cache_handle",
             &vfs::LocalPythonClient::remove_cluster_cache_handle,
             (bpy::args("handle")),
             "Remove a ClusterCacheHandle (and its entries) from the ClusterCache\n"
             "@param handle, ClusterCacheHandle\n"
             "@returns: a ClusterCacheHandleInfo object\n");
        ;
}

}

}
