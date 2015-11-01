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
