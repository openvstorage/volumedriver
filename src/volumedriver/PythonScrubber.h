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

#ifndef VD_PYTHON_SCRUBBER_H_
#define VD_PYTHON_SCRUBBER_H_

#include "ScrubberAdapter.h"

#include <string>

#include <boost/python/tuple.hpp>

namespace scrubbing
{

namespace python
{

struct Scrubber
    : private scrubbing::ScrubberAdapter
{
    static boost::python::tuple
    scrub(const std::string& scrub_work_str,
          const std::string& scratch_dir,
          const uint64_t region_size_exponent,
          const float fill_ratio,
          const bool apply_immediately,
          const bool verbose_scrubbing);

    static void
    registerize();
};

}

}


#endif // !VD_PYTHON_SCRUBBER_H_
