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
#include <youtils/python/IterableConverter.h>
#include <youtils/python/StringyConverter.h>

namespace youtils
{

namespace python
{

namespace ara = arakoon;
namespace bpy = boost::python;

DEFINE_PYTHON_WRAPPER(ArakoonClient)
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
