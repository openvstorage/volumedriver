// Copyright 2015 Open vStorage NV
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

#ifndef SCRUBBER_ADAPTER_H_
#define SCRUBBER_ADAPTER_H_
#include <utility>
#include <string>
#include "Types.h"
//#include <boost/python/tuple.hpp>

namespace scrubbing
{

class ScrubberAdapter
{

public:
    typedef std::pair<std::string, std::string> result_type;

    ScrubberAdapter();

    const static uint64_t region_size_exponent_default;
    const static float fill_ratio_default;
    const static bool apply_immediately_default;
    const static bool verbose_scrubbing_default;

    static result_type
    scrub_(const std::string& work_unit,
           const std::string& workdir,
           const uint64_t region_size_exponent = region_size_exponent_default,
           const float fill_ratio = fill_ratio_default,
           const bool apply_immediately = apply_immediately_default,
           const bool verbose_scrubbing = verbose_scrubbing_default);
};

}

#endif // SCRUBBER_ADAPTER_H_
