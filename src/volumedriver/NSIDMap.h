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

#ifndef NSIDMAP_H_
#define NSIDMAP_H_
#include "Types.h"
#include <map>
#include <string>
#include <algorithm>
#include "backend/BackendInterface.h"
#include <youtils/Logging.h>
#include <youtils/IOException.h>


namespace backend {class BackendInterface;}

namespace volumedriver
{
// Should be a vector<BackendInterfacePtr> with all constructors & manipulations keeping
// the datastructure sane

class NSIDMap
    : private std::vector<BackendInterfacePtr>
{
    DECLARE_LOGGER("NSIDMap");

public:
    inline static size_t max_size()
    {
        return std::numeric_limits<uint8_t>::max() + 1;
    }

    NSIDMap();

    NSIDMap&
    operator=(const NSIDMap& in_other) = delete;

    NSIDMap(const NSIDMap& in_other) = delete;

    NSIDMap&
    operator=(NSIDMap&& in_other);

    NSIDMap(NSIDMap&& in_other);

    size_t
    size() const;

    const BackendInterfacePtr&
    get(const SCOCloneID id) const;

    const BackendInterfacePtr&
    get(const uint8_t id) const;

    void
    set(const SCOCloneID id,
        BackendInterfacePtr bi);

    void
    set(const uint8_t id,
        BackendInterfacePtr bi);

    bool
    empty();

    SCOCloneID
    getCloneID(const Namespace& ns) const;
};


}

#endif // NSIDMAP_H_

// Local Variables: **
// mode: c++ **
// End: **
