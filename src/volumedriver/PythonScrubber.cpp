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

#include "ScrubReply.h"
#include "ScrubWork.h"
#include "PythonScrubber.h"

#include <iostream>
#include <string>
#include <vector>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/enum.hpp>

#include <youtils/OptionValidators.h>

namespace scrubbing
{

namespace python
{

namespace be = backend;
namespace bpy = boost::python;
namespace yt = youtils;

bpy::tuple
Scrubber::scrub(const std::string& scrub_work_str,
                const std::string& scratch_dir,
                const uint64_t region_size_exponent,
                const float fill_ratio,
                const bool apply_immediately,
                const bool verbose_scrubbing,
                const boost::optional<std::string>& backend_config)
{
    using namespace scrubbing;

    const ScrubWork work(scrub_work_str);

    std::unique_ptr<be::BackendConfig> bcfg;
    if (backend_config)
    {
        bcfg = be::BackendConfig::makeBackendConfig(yt::ConfigLocation(*backend_config));
    }

    const ScrubReply reply(ScrubberAdapter::scrub(std::move(bcfg),
                                                  work,
                                                  scratch_dir,
                                                  region_size_exponent,
                                                  fill_ratio,
                                                  apply_immediately,
                                                  verbose_scrubbing));

    return bpy::make_tuple(work.id_.str(),
                           reply.str());
}

void
Scrubber::registerize()
{
    using namespace boost::python;

    class_<Scrubber, boost::noncopyable>("Scrubber",
                                         "Scrubbing functionality for python",
                                         init<>(""))
        .def("scrub",
             &Scrubber::scrub,
             (args("work_unit"),
              args("scratch_dir"),
              args("region_size_exponent") = ScrubberAdapter::region_size_exponent_default,
              args("fill_ratio") = ScrubberAdapter::fill_ratio_default,
              args("apply_immediately") = ScrubberAdapter::apply_immediately_default,
              args("verbose_scrubbing") = ScrubberAdapter::verbose_scrubbing_default,
              args("backend_config") = boost::optional<std::string>()),
              "Scrubs a work unit and returns a scrub_result\n",
             "@param work_unit: a string, a opaque string that encodes the scrub work\n"
             "@param region_size_exponent: a number, "
             "region_size_exponent don't change from default if you don't know what you're doing, default 25\n"
             "@param scratch_dir: a string, the temporary directory to start the scrubbing under\n"
             "@param fill_ratio: a number, giving the sco fill ratio, default 0.9\n"
             "@param apply_immediately: a boolean, "
             "should be set to true only for scrubbing with PIT replicated volumes, default False\n"
             "@parm verbose_scrubbing: a boolean, whether the scrubbing should print it's intermediate result, default True\n"
             "@param backend_config: optional string, backend config location (file, etcd url, ...)\n"
             "@result a tuple of volume_id and a string that encodes the scrub result to apply")
        .staticmethod("scrub");
}

}

}
