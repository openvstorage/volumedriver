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

#include "ScrubReply.h"
#include "ScrubWork.h"
#include "PythonScrubber.h"

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/enum.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace scrubbing
{

namespace python
{

namespace bpy = boost::python;

bpy::tuple
Scrubber::scrub(const std::string& scrub_work_str,
                const std::string& scratch_dir,
                const uint64_t region_size_exponent,
                const float fill_ratio,
                const bool apply_immediately,
                const bool verbose_scrubbing)
{
    using namespace scrubbing;

    const ScrubWork work(scrub_work_str);
    const ScrubReply reply(ScrubberAdapter::scrub(work,
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
              args("verbose_scrubbing") = ScrubberAdapter::verbose_scrubbing_default),
              "Scrubs a work unit and returns a scrub_result\n",
             "@param work_unit: a string, a opaque string that encodes the scrub work\n"
             "@param region_size_exponent: a number, "
             "region_size_exponent don't change from default if you don't know what you're doing, default 25\n"
             "@param scratch_dir: a string, the temporary directory to start the scrubbing under\n"
             "@param fill_ratio: a number, giving the sco fill ratio, default 0.9\n"
             "@param apply_immediately: a boolean, "
             "should be set to true only for scrubbing with PIT replicated volumes, default False\n"
             "@parm verbose_scrubbing: a boolean, "
             "whether the scrubbing should print it's intermediate result, default True\n"
             "@result a tuple of volume_id and a string that encodes the scrub result to apply")
        .staticmethod("scrub");
}

}

}
