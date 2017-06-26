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

#ifndef SCRUBBER_ADAPTER_H_
#define SCRUBBER_ADAPTER_H_

#include "Types.h"

#include <utility>
#include <string>

#include <boost/filesystem.hpp>

namespace backend
{
struct ConnectionManagerParameters;
}

namespace scrubbing
{

class ScrubReply;
class ScrubWork;

struct ScrubberAdapter
{
    ScrubberAdapter() = default;

    ~ScrubberAdapter() = default;

    ScrubberAdapter(const ScrubberAdapter&) = default;

    ScrubberAdapter&
    operator=(const ScrubberAdapter&) = default;

    const static uint64_t region_size_exponent_default;
    const static float fill_ratio_default;
    const static bool apply_immediately_default;
    const static bool verbose_scrubbing_default;

    static ScrubReply
    scrub(std::unique_ptr<backend::BackendConfig>,
          const backend::ConnectionManagerParameters&,
          const ScrubWork&,
          const boost::filesystem::path& workdir,
          const uint64_t region_size_exponent = region_size_exponent_default,
          const float fill_ratio = fill_ratio_default,
          const bool apply_immediately = apply_immediately_default,
          const bool verbose_scrubbing = verbose_scrubbing_default);
};

}

#endif // SCRUBBER_ADAPTER_H_
