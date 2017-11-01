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

#include "MDSClient.h"

#include <boost/python/class.hpp>
#include <boost/python/enum.hpp>

#include <youtils/Logger.h>
#include <youtils/python/IterableConverter.h>

#include <volumedriver/ClusterLocationAndHash.h>
#include <volumedriver/MDSNodeConfig.h>
#include <volumedriver/OwnerTag.h>
#include <volumedriver/metadata-server/Interface.h>
#include <volumedriver/metadata-server/PythonClient.h>
#include <volumedriver/python/SCOAdapter.h>

namespace bpy = boost::python;
namespace mds = metadata_server;
namespace vd = volumedriver;
namespace vpy = volumedriver::python;
namespace ypy = youtils::python;
namespace yt = youtils;

namespace volumedriverfs
{

namespace python
{

namespace
{

template<typename T>
std::string
repr(T* t)
{
    return boost::lexical_cast<std::string>(*t);
}

}

void
MDSClient::registerize()
{
    ypy::register_once<vpy::SCOAdapter>();

        bpy::enum_<mds::Role>("Role")
        .value("Master", mds::Role::Master)
        .value("Slave", mds::Role::Slave)
        ;

    bpy::class_<mds::TableCounters>("MDSTableCounters",
                                    "MDS TableCounters",
                                    bpy::no_init)
        .def("__eq__",
             &mds::TableCounters::operator==)
        .def("__repr__",
             &repr<mds::TableCounters>)
        .def("__str__",
             &repr<mds::TableCounters>)
#define DEF_READONLY(name)                              \
        .def_readonly(#name, &mds::TableCounters::name)

        DEF_READONLY(total_tlogs_read)
        DEF_READONLY(incremental_updates)
        DEF_READONLY(full_rebuilds)

#undef DEF_READONLY
        ;

    vd::SCOOffset (vd::ClusterLocation::*get_loc_offset)() const = &vd::ClusterLocation::offset;

    bpy::class_<vd::ClusterLocation>("ClusterLocation",
                                     "A volumedriver ClusterLocation",
                                     bpy::no_init)
        .def("__repr__",
             &repr<vd::ClusterLocation>)
        .def("__str__",
             &repr<vd::ClusterLocation>)
        .def("offset",
             get_loc_offset,
             "offset")
        .def("sco",
             &vd::ClusterLocation::sco,
             "SCO")
        ;

    bpy::class_<vd::ClusterLocationAndHash>("ClusterLocationAndHash",
                                            "A volumedriver ClusterLocationAndHash",
                                            bpy::no_init)
        .def("__repr__",
             &repr<vd::ClusterLocationAndHash>)
        .def("__str__",
             &repr<vd::ClusterLocationAndHash>)
        .add_property("location",
                      bpy::make_getter(&vd::ClusterLocationAndHash::clusterLocation,
                                       bpy::return_value_policy<bpy::return_by_value>()))
        ;

    REGISTER_ITERABLE_CONVERTER(std::vector<vd::ClusterLocationAndHash>);

    bpy::class_<mds::PythonClient>("MDSClient",
                                   "management and monitoring of MetaDataServer (MDS)",
                                   bpy::init<const vd::MDSNodeConfig&,
                                             unsigned>
                                   ((bpy::args("mds_node_config"),
                                     bpy::args("timeout_secs") = vd::MDSMetaDataBackendConfig::default_timeout_secs_),
                                   "Create an MDSClient for the given server\n"
                                    "@param: mds_node_config: MDSNodeConfig pointing to the desired MDS\n"
                                    "@param: timeout_secs: unsigned, timeout (in seconds) for remote MDS calls\n"))
        .def("create_namespace",
             &mds::PythonClient::create_namespace,
             "Create a namespace on an MDS.\n"
             "@param: nspace: string, namespace name\n"
             "@returns: eventually\n")
        .def("list_namespaces",
             &mds::PythonClient::list_namespaces,
             "Get a list of all active namespaces on an MDS.\n"
             "@returns: [ string ], namespace names\n")
        .def("remove_namespace",
             &mds::PythonClient::remove_namespace,
             "Remove a namespace from an MDS.\n"
             "@param: nspace: string, namespace name\n"
             "@returns: eventually\n")
        .def("get_role",
             &mds::PythonClient::get_role,
             (bpy::args("nspace")),
             "Get the Role of a namespace on that MDS.\n"
             "@param: nspace: string, namespace name\n"
             "@returns: Role enum value\n")
        .def("set_role",
             &mds::PythonClient::set_role,
             (bpy::args("nspace"),
              bpy::args("role"),
              bpy::args("owner_tag") = vd::OwnerTag(0)),
             "Set the Role of a namespace on that MDS.\n"
             "@param: nspace: string, namespace name\n"
             "@param: role: Role enum value, role to assume\n"
             "@param: owner_tag: Optional OwnerTag, tag that identifies the current owner / fences old owners\n"
             "@returns: eventually\n")
        .def("get_cork_id",
             &mds::PythonClient::get_cork_id,
             (bpy::args("nspace")),
             "Get the current cork ID of an MDS namespace.\n"
             "@param: nspace: string, namespace name\n"
             "@returns: string or None, cork UUID (if present)\n")
        .def("get_scrub_id",
             &mds::PythonClient::get_scrub_id,
             (bpy::args("nspace")),
             "Get the current scrub ID of an MDS namespace.\n"
             "@param: nspace: string, namespace name\n"
             "@returns: string or None, scrub UUID (if present)\n")
        .def("catch_up",
             &mds::PythonClient::catch_up,
             (bpy::args("nspace"),
              bpy::args("dry_run")),
             "Force a slave namespace to catch up with the metadata on the backend.\n"
             "@param: nspace: string, namespace name\n"
             "@param: dry_run: boolean, whether to actually apply the necessary changes\n"
             "@returns: uint, the number of TLogs that had/have to be applied\n")
        .def("get_table_counters",
             &mds::PythonClient::get_table_counters,
             (bpy::args("nspace"),
              bpy::args("reset")),
             "Retrieve MDSTableCounters for the given namespace.\n"
             "@param: nspace: string, namespace name\n"
             "@param: reset: boolean, whether to reset the counters after retrieval\n"
             "@returns: MDSTableCounters\n")
        .def("get_owner_tag",
             &mds::PythonClient::owner_tag,
             (bpy::args("nspace")),
             "Retrieve the current OwnerTag for the given namespace.\n"
             "@param: nspace: string, namespace name\n"
             "@returns: OwnerTag\n")
        .def("_get_page",
             &mds::PythonClient::get_page,
             (bpy::args("nspace"),
              bpy::args("page_address")),
             "Retrieve the metadata page for the given cluster address.\n"
             "@param: nspace: string, namespace name\n"
             "@param: page address\n"
             "@returns: [ ClusterLocationAndHash ]\n")
        ;
}

}

}
