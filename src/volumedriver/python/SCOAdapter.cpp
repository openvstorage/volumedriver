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

#include "SCOAdapter.h"

#include "../SCO.h"
#include "../Types.h"

#include <boost/lexical_cast.hpp>
#include <boost/python.hpp>
#include <boost/python/args.hpp>
#include <boost/python/class.hpp>

#include <youtils/python/StrongArithmeticTypedefConverter.h>

namespace volumedriver
{

namespace python
{

namespace bpy = boost::python;

DEFINE_PYTHON_WRAPPER(SCOAdapter)
{
    REGISTER_STRONG_ARITHMETIC_TYPEDEF_CONVERTER(SCOCloneID);
    REGISTER_STRONG_ARITHMETIC_TYPEDEF_CONVERTER(SCOVersion);

    SCOCloneID (SCO::*get_sco_clone_id)() const = &SCO::cloneID;
    SCONumber (SCO::*get_sco_number)() const = &SCO::number;
    SCOVersion (SCO::*get_sco_version)() const = &SCO::version;

    bpy::class_<SCO>("SCO",
                     "A volumedriver SCO",
                     bpy::init<const std::string&>
                     (bpy::args("string"),
                      "Constructor on the basis of a string (XX_XXXXXXXX_XX)"))
        .def("__repr__",
             &boost::lexical_cast<std::string, const SCO&>)
        .def("__str__",
             &boost::lexical_cast<std::string, const SCO&>)
        .def("clone_id",
             get_sco_clone_id,
             "SCO clone ID")
        .def("number",
             get_sco_number,
             "SCO number")
        .def("version",
             get_sco_version,
             "SCO version")
        .def("asBool",
             &SCO::asBool,
             "SCO 0 is never used as a storage unit")
        .def("isSCOString",
             &SCO::isSCOString,
             "test whether a string can be interpreted as a SCO string")
        .staticmethod("isSCOString")
        // backward compatibility with the ToolCut version - kill these eventually
        .def("cloneID",
             get_sco_clone_id,
             "Get the CloneID of the SCO (deprecated, use clone_id instead!)")
        .def("str",
             &SCO::str,
             "Get the stringified form of the SCO (deprecated, use the free str() function instead!)")
        ;
}

}

}
