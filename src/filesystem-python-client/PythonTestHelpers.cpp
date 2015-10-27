// Copyright 2015 iNuron NV
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

#include "PythonTestHelpers.h"

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
        ;
}

}

}
