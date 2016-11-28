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

#include "WriteSCOCache.h"
#include "ClusterLocation.h"
#include "OpenSCO.h"
#include "CachedSCO.h"

#define LOCK()                                  \
    boost::lock_guard<decltype(lock_)> sl__(lock_)

#ifndef NDEBUG
#define ASSERT_LOCKED()                         \
    VERIFY(not lock_.tryLock())
#else
#define ASSERT_LOCKED()
#endif

namespace volumedriver
{

WriteSCOCache::WriteSCOCache(size_t size)
    : std::vector<OpenSCOPtr>(size)
{}

size_t
WriteSCOCache::index_(SCO sco) const
{
    return sco.number() % size();
}

OpenSCOPtr
WriteSCOCache::find(SCO sco) const
{
    LOCK();
    OpenSCOPtr osco = operator[](index_(sco));
    if (osco != 0 and osco->sco_name() == sco)
    {
        return osco;
    }
    else
    {
        return 0;
    }
}

void
WriteSCOCache::insert(OpenSCOPtr osco)
{
    LOCK();
    SCO sco = osco->sco_name();
    erase_bucket_(sco);
    operator[](index_(sco)) = osco;
}

void
WriteSCOCache::erase(SCO sco,
                     bool sync)
{
    LOCK();
    size_t idx = index_(sco);
    OpenSCOPtr osco = operator[](idx);
    if (osco != 0 and osco->sco_name() == sco)
    {
        if (sync)
        {
            osco->sync();
        }
        operator[](idx) = 0;
    }
}

void
WriteSCOCache::erase_bucket(SCO sco)
{
    LOCK();
    erase_bucket_(sco);
}

void
WriteSCOCache::erase_bucket_(SCO sco)
{
    ASSERT_LOCKED();
    size_t idx = index_(sco);

    if (operator[](idx) != 0)
    {
        operator[](idx)->sync();
        operator[](idx) = 0;
    }
}

std::vector<OpenSCOPtr>::iterator
WriteSCOCache::begin()
{
    return std::vector<OpenSCOPtr>::begin();
}

std::vector<OpenSCOPtr>::iterator
WriteSCOCache::end()
{
    return std::vector<OpenSCOPtr>::end();
}

}

// Local Variables: **
// mode: c++ **
// End: **
