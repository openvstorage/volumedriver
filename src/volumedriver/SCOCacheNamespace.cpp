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
