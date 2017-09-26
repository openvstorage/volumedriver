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

#include "ScrubWork.h"

#include <boost/noncopyable.hpp>
#include <boost/python/class.hpp>

#include <volumedriver/ScrubWork.h>

namespace volumedriverfs
{

namespace python
{

namespace bpy = boost::python;

namespace
{

std::string
scrub_work_namespace(const scrubbing::ScrubWork& w)
{
    return w.ns_.str();
}

std::string
scrub_work_volume_id(const scrubbing::ScrubWork& w)
{
    return w.id_.str();
}

uint32_t
scrub_work_cluster_exponent(const scrubbing::ScrubWork& w)
{
    return w.cluster_exponent_;
}

uint32_t
scrub_work_sco_size(const scrubbing::ScrubWork& w)
{
    return w.sco_size_;
}

std::string
scrub_work_snapshot_name(const scrubbing::ScrubWork& w)
{
    return w.snapshot_name_.str();
}

}

void
ScrubWork::registerize()
{
    bpy::class_<scrubbing::ScrubWork,
                boost::noncopyable>("ScrubWork",
                                    "inspect scrubbing work units",
                                    bpy::init<const std::string&>
                                    ((bpy::args("scrub_work_str")),
                                     "Create ScrubWork from its serialized representation\n"
                                     "@param scrub_work_str, string, serialized representation of ScrubWork as returned by LockedClient\n"))
        .def("__repr__",
             &scrubbing::ScrubWork::str)
        .def("namespace",
             &scrub_work_namespace)
        .def("volume_id",
             &scrub_work_volume_id)
        .def("cluster_exponent",
             &scrub_work_cluster_exponent)
        .def("sco_size",
             &scrub_work_sco_size)
        .def("snapshot_name",
             &scrub_work_snapshot_name)
        ;
}

}

}
