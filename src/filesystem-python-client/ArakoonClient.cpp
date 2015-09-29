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

#include "ArakoonClient.h"
#include "IterableConverter.h"
#include "StringyConverter.h"

#include <boost/python/class.hpp>
#include <boost/python/copy_const_reference.hpp>
#include <boost/python/def.hpp>
#include <boost/python/enum.hpp>
#include <boost/python/module.hpp>
#include <boost/python/exception_translator.hpp>
#include <boost/python/object.hpp>
#include <boost/python/register_ptr_to_python.hpp>
#include <boost/python/return_value_policy.hpp>

#include <youtils/ArakoonNodeConfig.h>

namespace volumedriverfs
{

namespace python
{

namespace ara = arakoon;
namespace bpy = boost::python;
namespace yt = youtils;

void
ArakoonClient::registerize()
{
    REGISTER_STRINGY_CONVERTER(ara::ClusterID);
    REGISTER_STRINGY_CONVERTER(ara::NodeID);

    bpy::class_<ara::ArakoonNodeConfig>("ArakoonNodeConfig",
                                        "Arakoon node configuration",
                                        bpy::init<const ara::NodeID&,
                                        const std::string&,
                                        const uint16_t>((bpy::args("node_id"),
                                                         bpy::args("host"),
                                                         bpy::args("port")),
                                                        "Create an Arakoon node configuration\n"
                                                        "@param node_id: string, Arakoon node ID\n"
                                                        "@param host: string, hostname or IP address\n"
                                                        "@param port: uint16_t, TCP port\n"))
        .def("__eq__", &ara::ArakoonNodeConfig::operator==)
        .def("__str__", &ara::ArakoonNodeConfig::str)
        .def("__repr__", &ara::ArakoonNodeConfig::str)
        .def_readonly("node_id", &ara::ArakoonNodeConfig::node_id_)
        .def_readonly("host", &ara::ArakoonNodeConfig::hostname_)
        .def_readonly("port", &ara::ArakoonNodeConfig::port_);

    REGISTER_ITERABLE_CONVERTER(std::vector<ara::ArakoonNodeConfig>);
}

}

}
