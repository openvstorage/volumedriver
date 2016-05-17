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
