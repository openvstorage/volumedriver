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

#ifndef WRITE_SCO_CACHE_H_
#define WRITE_SCO_CACHE_H_

#include <boost/utility.hpp>
#include <vector>

#include <youtils/Logging.h>

#include "Types.h"
#include "SCO.h"
#include <youtils/SpinLock.h>

namespace volumedriver
{
class WriteSCOCache
    : private std::vector<OpenSCOPtr>
{
public:
    typedef std::vector<OpenSCOPtr>::iterator iterator;

    explicit WriteSCOCache(size_t size);

    WriteSCOCache(const WriteSCOCache&) = delete;
    WriteSCOCache& operator=(const WriteSCOCache&) = delete;

    OpenSCOPtr
    find(SCO sco) const;

    void
    insert(OpenSCOPtr osco);

    // remove the OpenSCOPtr for SCO if present
    void
    erase(SCO sco,
          bool sync = true);

    // clear the cache bucket SCO maps to (no matter if the SCO is
    // really there!)
    void
    erase_bucket(SCO sco);

    // kill these - see DataStoreNG::sync_()
    iterator
    begin();

    iterator
    end();

private:
    DECLARE_LOGGER("WriteSCOCache");

    mutable fungi::SpinLock lock_;

    size_t
    index_(SCO sco) const;

    void
    erase_bucket_(SCO sco);
};

}

#endif // !WRITE_SCO_CACHE_H_

// Local Variables: **
// mode: c++ **
// End: **
