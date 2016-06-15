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

#include "SCOCacheNamespace.h"
#include "ClusterLocation.h"

namespace volumedriver
{

SCOCacheNamespace::SCOCacheNamespace(const backend::Namespace& nspace,
                                     uint64_t min,
                                     uint64_t max_non_disposable)
  : std::map<const SCO, SCOCacheNamespaceEntry>()
  , nspace_(nspace)
  , min_(min)
  , max_non_disposable_(max_non_disposable)
  , choking_(false)
{
    LOG_DEBUG(nspace_ << ": created");
}

SCOCacheNamespace::~SCOCacheNamespace()
{
    clear();
    LOG_DEBUG(nspace_ << ": destroying");
}

uint64_t
SCOCacheNamespace::getMinSize() const
{
    return min_;
}

uint64_t
SCOCacheNamespace::getMaxNonDisposableSize() const
{
    return max_non_disposable_;
}

const backend::Namespace&
SCOCacheNamespace::getName() const
{
    return nspace_;
}

void
SCOCacheNamespace::setLimits(uint64_t min, uint64_t max_non_disposable)
{
    min_ = min;
    max_non_disposable_ = max_non_disposable;
}

void
SCOCacheNamespace::setLimitMax(uint64_t max_non_disposable)
{
    max_non_disposable_ = max_non_disposable;
}

SCOCacheNamespaceEntry*
SCOCacheNamespace::findEntry(SCO scoName)
{
    iterator it = find(scoName);
    if (it == end())
    {
        return 0;
    }

    return &(it->second);
}

SCOCacheNamespaceEntry*
SCOCacheNamespace::findEntry_throw(SCO scoName)
{
    SCOCacheNamespaceEntry* e = findEntry(scoName);
    if (e == 0)
    {
        std::string msg = "no such entry: " + scoName.str();
        throw fungi::IOException(msg.c_str(),
                                 nspace_.c_str(),
                                 ENOENT);
    }

    return e;
}

bool
SCOCacheNamespace::isChoking() const
{
    return choking_;
}

void
SCOCacheNamespace::setChoking(bool choking)
{
    choking_ = choking;
}

}

// Local Variables: **
// mode: c++ **
// End: **
