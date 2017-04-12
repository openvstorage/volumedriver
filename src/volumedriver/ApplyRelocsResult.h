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

#ifndef VD_APPLY_RELOCS_RESULT_H_
#define VD_APPLY_RELOCS_RESULT_H_

#include <chrono>
#include <functional>
#include <vector>

#include <boost/optional.hpp>

namespace volumedriver
{

using ApplyRelocsContinuation =
    std::function<void(const boost::optional<std::chrono::seconds>& timeout)>;

using ApplyRelocsContinuations = std::vector<ApplyRelocsContinuation>;

using ApplyRelocsResult = std::tuple<ApplyRelocsContinuations, uint64_t>;

}

#endif // !VD_APPLY_RELOCS_RESULT_H_
