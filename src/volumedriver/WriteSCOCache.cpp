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

#include "WriteSCOCache.h"
#include "ClusterLocation.h"
#include "OpenSCO.h"
#include "CachedSCO.h"

#define LOCK()                                  \
    fungi::ScopedSpinLock sl__(lock_)

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
