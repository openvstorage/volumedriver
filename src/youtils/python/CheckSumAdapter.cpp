// Copyright (C) 2018 iNuron NV
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

#include "CheckSumAdapter.h"

#include "../CheckSum.h"

#include <boost/lexical_cast.hpp>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>

namespace youtils
{

namespace python
{

namespace
{

namespace bpy = boost::python;
using namespace std::literals::string_literals;

std::string
str(const CheckSum& cs)
{
    // push this / only the '0x' prefix to operator<<(std::ostream&, CheckSum)?
    return "CheckSum(0x"s + boost::lexical_cast<std::string>(cs) + ")"s;
}

void
update(CheckSum& cs,
       const std::string& s)
{
    cs.update(s.data(),
              s.size());
}

}

DEFINE_PYTHON_WRAPPER(CheckSumAdapter)
{
    bpy::class_<CheckSum>("CheckSum",
                          "CheckSum (CRC32c) utility",
                          bpy::init<>())
        .def("__repr__",
             &str)
        .def("__str__",
             &str)
        .def("update",
             &update,
             (bpy::args("buf")),
             "Update checksum based on buf\n"
             "@param buf, string (binary) containing the data\n")
        .def("value",
             &CheckSum::getValue,
             "Get checksum value\n"
             "@return: checksum value (u32)\n")
        ;
}

}

}
