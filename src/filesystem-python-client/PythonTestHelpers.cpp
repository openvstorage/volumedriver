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

#include "PythonTestHelpers.h"

#include <boost/chrono.hpp>
#include <boost/python/class.hpp>

#include <youtils/ArakoonNodeConfig.h>

#include <volumedriver/Types.h>

#include <filesystem/PythonClient.h>

namespace volumedriverfs
{

namespace python
{

namespace ara = arakoon;
namespace bpy = boost::python;
namespace vd = volumedriver;
namespace yt = youtils;

std::string
PythonTestHelpers::generate_exceptions(uint32_t type)
{
    switch(type)
    {
    case 1:
        throw clienterrors::MaxRedirectsExceededException("the_method", "127.0.0.1", 12345);
    case 2:
        throw clienterrors::ObjectNotFoundException("some_vol");
    case 3:
        throw clienterrors::InvalidOperationException("some_vol");
    case 4:
        throw clienterrors::SnapshotNotFoundException("some_snap");
    case 5:
        throw clienterrors::FileExistsException("some_vol");
    case 6:
        throw clienterrors::InsufficientResourcesException("some_vol");
    }
    return "no problem";
}

std::map<NodeId, ClusterNodeStatus::State>
PythonTestHelpers::generate_node_status_map()
{
    std::map<NodeId, ClusterNodeStatus::State> m;
    m[NodeId("A")] = ClusterNodeStatus::State::Online;
    m[NodeId("B")] = ClusterNodeStatus::State::Offline;
    return m;
}

yt::UpdateReport
PythonTestHelpers::generate_update_report()
{
    yt::UpdateReport rep;
    rep.update("updated_param_1", "old_value_1", "new_value_1");
    rep.update("updated_param_2", "old_value_2", "new_value_2");

    rep.no_update("param_1", "value_1", "value_1");
    rep.no_update("param_2", "value_2", "value_2");

    return rep;
}

void
PythonTestHelpers::registerize()
{
    bpy::class_<PythonTestHelpers>("PythonTestHelpers")
        .def("generate_exceptions",
             &PythonTestHelpers::generate_exceptions,
             (bpy::args("exception_type")))
        .staticmethod("generate_exceptions")
        .def("generate_node_status_map",
             &PythonTestHelpers::generate_node_status_map)
        .staticmethod("generate_node_status_map")
        .def("generate_update_report",
             &PythonTestHelpers::generate_update_report)
        .staticmethod("generate_update_report")
        .def("reflect_arakoon_node_id",
             &PythonTestHelpers::reflect<ara::NodeID>)
        .staticmethod("reflect_arakoon_node_id")
        .def("reflect_arakoon_node_config",
             &PythonTestHelpers::reflect<ara::ArakoonNodeConfig>)
        .staticmethod("reflect_arakoon_node_config")
        .def("reflect_cluster_node_config",
             &PythonTestHelpers::reflect<ClusterNodeConfig>)
        .staticmethod("reflect_cluster_node_config")
        .def("reflect_cluster_node_config_list",
             &PythonTestHelpers::reflect<std::vector<ClusterNodeConfig>>)
        .staticmethod("reflect_cluster_node_config_list")
        .def("reflect_loglevel",
             &PythonTestHelpers::reflect<yt::Severity>)
        .staticmethod("reflect_loglevel")
        .def("reflect_logfilter",
             &PythonTestHelpers::reflect<std::pair<std::string, yt::Severity>>)
        .staticmethod("reflect_logfilter")
        .def("reflect_logfilters",
             &PythonTestHelpers::reflect<std::vector<std::pair<std::string, yt::Severity>>>)
        .staticmethod("reflect_logfilters")
        .def("reflect_maybe_volume_id",
             &PythonTestHelpers::reflect<boost::optional<vd::VolumeId>>)
        .staticmethod("reflect_maybe_volume_id")
        .def("stringy_things",
             &PythonTestHelpers::stringy_things)
        .staticmethod("stringy_things")
        .def("arithmeticy_things",
             &PythonTestHelpers::arithmeticy_things)
        .staticmethod("arithmeticy_things")
        .def("reflect_maybe_chrono_seconds",
             &PythonTestHelpers::reflect<boost::optional<boost::chrono::seconds>>)
        .staticmethod("reflect_maybe_chrono_seconds")
        .def("reflect_dimensioned_value",
             &PythonTestHelpers::reflect<yt::DimensionedValue>)
        .staticmethod("reflect_dimensioned_value")
        ;
}

}

}
