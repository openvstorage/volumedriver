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
#include "SCOAdapter.h"

#include "../ClusterLocation.h"
#include "../Types.h"

#include <boost/python.hpp>
#include <boost/python/class.hpp>

namespace volumedriver
{

namespace python
{

namespace bpy = boost::python;
namespace ypy = youtils::python;

namespace
{
bool
is_cluster_location_string(const std::string& s)
{
    return ClusterLocation::isClusterLocationString(s,
                                                    true);
}

}

DEFINE_PYTHON_WRAPPER(ClusterLocationAdapter)
{
    ypy::register_once<SCOAdapter>();

    SCOCloneID (ClusterLocation::*get_clone_id)() const = &ClusterLocation::cloneID;
    SCONumber (ClusterLocation::*get_number)() const = &ClusterLocation::number;
    SCOOffset (ClusterLocation::*get_offset)() const = &ClusterLocation::offset;
    SCOVersion (ClusterLocation::*get_version)() const = &ClusterLocation::version;

    bpy::class_<ClusterLocation>("ClusterLocation",
                                 "Interface to ClusterLocation i.e. a SCO and an offset in that SCO",
                                 bpy::init<const std::string&>(bpy::arg("string"),
                                                               "Constructor from a string (XX_XXXXXXXX_XX:XXXX)"))
        .def(bpy::init<const SCO&, SCOOffset>((bpy::args("sco"),
                                               bpy::args("offset")),
                                              "Constructor from a SCO + offset"))
        .def("__str__",
             &ClusterLocation::str)
        .def("__repr__",
             &ClusterLocation::str)
        .def("sco",
             &ClusterLocation::sco,
             "Get the SCO associated with this ClusterLocation")
        .def("offset",
             get_offset,
             "Get the offset in the SCO")
        .def("isClusterLocationString",
             &is_cluster_location_string,
             "Test whether the string is the standard representation of a ClusterLocation")
        .staticmethod("isClusterLocationString")
        // backward compatibility with the ToolCut version - kill these eventually
        .def("version",
             get_version,
             "Get the version of the SCO (deprecated, use sco() and the respective method on it instead!)")
        .def("cloneID",
             get_clone_id,
             "Get the cloneID of the SCO (deprecated, use sco() and the respective method on it instead!)")
        .def("number",
             get_number,
             "Get the number of the SCO (deprecated, use sco() and the respective method on it instead!)")
        .def("str",
             &ClusterLocation::str,
             "Get the stringified form of the ClusterLocation (deprecated, use the free str() function instead!)")
        ;
}

}

}
