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
