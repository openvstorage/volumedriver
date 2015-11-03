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

#include "LockedClient.h"

#include <boost/noncopyable.hpp>
#include <boost/python/class.hpp>

#include <volumedriver/ScrubberAdapter.h>

#include <filesystem/LockedPythonClient.h>

namespace volumedriverfs
{

namespace python
{

namespace bpy = boost::python;
namespace vfs = volumedriverfs;

void
LockedClient::registerize()
{
    bpy::class_<vfs::LockedPythonClient,
                bpy::bases<vfs::PythonClient>,
                boost::noncopyable,
                vfs::LockedPythonClient::Ptr>("LockedClient",
                     "Context manager for synchronized namespace access",
                     bpy::no_init)
        .def("__enter__",
             &vfs::LockedPythonClient::enter)
        .def("__exit__",
             &vfs::LockedPythonClient::exit)
                .def("get_scrubbing_workunits",
             &LockedPythonClient::get_scrubbing_work,
             "get a list of scrubbing work units -- opaque strings\n"
             "@returns: list of strings\n"
             "@raises \n"
             "      ObjectNotFoundException\n"
             "      InvalidOperationException (on template)\n")
        .def("apply_scrubbing_result",
             &vfs::LockedPythonClient::apply_scrubbing_result,
             (bpy::args("scrubbing_work_result")),
             "Apply a scrubbing result on the volume it's meant for\n"
             "@param scrubbing work result an opaque tuple returned by the scrubber\n"
             "@raises \n"
             "      ObjectNotFoundException\n"
             "      SnapshotNotFoundException\n"
             "      InvalidOperationException (on template)\n")
        .def("scrub",
             &vfs::LockedPythonClient::scrub,
             (bpy::args("work_unit"),
              bpy::args("scratch_dir"),
              bpy::args("region_size_exponent") = scrubbing::ScrubberAdapter::region_size_exponent_default,
              bpy::args("fill_ratio") = scrubbing::ScrubberAdapter::fill_ratio_default,
              bpy::args("verbose_scrubbing") = scrubbing::ScrubberAdapter::verbose_scrubbing_default,
              bpy::args("scrubber_binary") = "ovs_scrubber"),
              "Scrubs a work unit and returns a scrub_result\n",
             "@param work_unit: a string, a opaque string that encodes the scrub work\n"
             "@param region_size_exponent: a number, "
             "region_size_exponent don't change from default if you don't know what you're doing, default 25\n"
             "@param scratch_dir: a string, the temporary directory to start the scrubbing under\n"
             "@param fill_ratio: a number, giving the sco fill ratio, default 0.9\n"
             "@param verbose_scrubbing: a boolean, "
             "whether the scrubbing should print it's intermediate result, default True\n"
             "@param scrubber_binary: string, scrubber binary to use\n"
             "@result a (n opaque) string that encodes the scrub result to apply")
        ;
}

}

}
