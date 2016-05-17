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

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/enum.hpp>
#include <boost/python/module.hpp>
#include "Bug2.h"

BOOST_PYTHON_MODULE(Bug2)
{
    using namespace boost::python;

    class_<Bug2,
           boost::noncopyable>("Bug2",
                               init<>
                               ("Creates a Bug1"))
                               .def("call", &Bug2::call)
        .def("call", &Bug2::call)
        .def("__str__",  &Bug2::str)
        .def("__repr__", &Bug2::str)
        ;
}

// Local Variables: **
// End: **
