// Copyright (C) 2017 iNuron NV
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

#include "ClusterLocationAdapter.h"
#include "ClusterLocationAndHashAdapter.h"

#include "../ClusterLocationAndHash.h"

#include <boost/lexical_cast.hpp>
#include <boost/python.hpp>
#include <boost/python/class.hpp>

namespace volumedriver
{

namespace python
{

namespace bpy = boost::python;
namespace ypy = youtils::python;

DEFINE_PYTHON_WRAPPER(ClusterLocationAndHashAdapter)
{
    ypy::register_once<ClusterLocationAdapter>();

    bpy::class_<ClusterLocationAndHash>("ClusterLocationAndHash",
                                        "A volumedriver ClusterLocationAndHash",
                                        bpy::no_init)
        .def("__repr__",
             boost::lexical_cast<std::string, const ClusterLocationAndHash&>)
        .def("__str__",
             boost::lexical_cast<std::string, const ClusterLocationAndHash&>)
        .add_property("location",
                      bpy::make_getter(&ClusterLocationAndHash::clusterLocation,
                                       bpy::return_value_policy<bpy::return_by_value>()))
        .def("use_hash",
             &ClusterLocationAndHash::use_hash,
             "Whether the per cluster hash is supported or not")
        .staticmethod("use_hash")
        ;
}

}

}
