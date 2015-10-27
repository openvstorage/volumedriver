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

#ifndef SCO_CACHE_NAMESPACE_H_
#define SCO_CACHE_NAMESPACE_H_

#include <map>
#include <string>

#include <stdint.h>
#include <boost/tuple/tuple.hpp>
#include <youtils/Logging.h>
#include "CachedSCO.h"


namespace volumedriver
{

class SCOCacheNamespaceEntry
{
public:
    SCOCacheNamespaceEntry(CachedSCOPtr sco, bool blocked)
        : sco_(sco)
        , blocked_(blocked)
    {}

    ~SCOCacheNamespaceEntry()
    {}

    bool
    isBlocked() const
    {
        return blocked_;
    }

    void
    setBlocked(bool blocked)
    {
        blocked_ = blocked;
    }

    CachedSCOPtr
    getSCO()
    {
        return sco_;
    }

private:
    CachedSCOPtr sco_;
    bool blocked_;
};

class SCOCacheNamespace
    : public std::map<const SCO, SCOCacheNamespaceEntry>
{
public:

    SCOCacheNamespace(const backend::Namespace& nspace,
                      uint64_t min,
                      uint64_t max_non_disposable);

    SCOCacheNamespace(const SCOCacheNamespace&) = delete;
    SCOCacheNamespace& operator=(const SCOCacheNamespace&) = delete;

    ~SCOCacheNamespace();

    uint64_t
    getMinSize() const;

    uint64_t
    getMaxNonDisposableSize() const;

    const Namespace&
    getName() const;

    void
    setLimits(uint64_t min,
              uint64_t max_non_disposable);

    void
    setLimitMax(uint64_t max_non_disposable);

    SCOCacheNamespaceEntry*
    findEntry(SCO scoName);

    SCOCacheNamespaceEntry*
    findEntry_throw(SCO scoName);

    void
    setChoking(bool choking);

    bool
    isChoking() const;

private:
    DECLARE_LOGGER("SCOCacheNamespace");

    const Namespace nspace_;
    uint64_t min_;
    uint64_t max_non_disposable_;
    bool choking_;
};

}

#endif // !SCO_CACHE_NAMESPACE_

// Local Variables: **
// mode: c++ **
// End: **
