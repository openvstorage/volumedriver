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
