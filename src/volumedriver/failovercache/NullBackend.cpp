// Copyright (C) 2017 iNuron NV
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

#include "NullBackend.h"
#include "../FailOverCacheEntry.h"

namespace volumedriver
{

namespace failovercache
{

namespace vd = volumedriver;

NullBackend::NullBackend(const std::string& nspace,
                         const vd::ClusterSize csize)
    : Backend(nspace,
              csize)
{}

void
NullBackend::open(const volumedriver::SCO sco)
{
    LOG_INFO(getNamespace() << ": opening " << sco);

    SCOEntriesPtr ptr = std::make_shared<SCOEntries>();
    const auto res(entries_.emplace(std::make_pair(sco,
                                                   ptr)));
    VERIFY(res.second);
    current_ = ptr;
}

void
NullBackend::close()
{
    current_ = nullptr;
}

void
NullBackend::add_entries(std::vector<vd::FailOverCacheEntry> entries,
                         std::unique_ptr<uint8_t[]>)
{
    VERIFY(current_);

    const size_t cap = current_->capacity();
    const size_t size = current_->size();

    if (size + 1 > cap)
    {
        current_->reserve(cap + sco_entries_slab_size_);
    }

    for (auto& e : entries)
    {
        e.buffer_ = nullptr;
    }

    current_->emplace_back(std::move(entries));
}

void
NullBackend::flush()
{}

void
NullBackend::remove(const volumedriver::SCO sco)
{
    LOG_INFO(getNamespace() << ": removing " << sco);

    entries_.erase(sco);
}

size_t
NullBackend::get_entries(const volumedriver::SCO sco,
                         const SCOOffset off,
                         const size_t max,
                         Backend::EntryProcessorFun& fun)
{
    LOG_INFO(getNamespace() << ": processing " << sco);

    const std::vector<uint8_t> vec(cluster_size(),
                                   0);
    size_t count = 0;
    const ClusterLocation start(sco,
                                off);

    const auto it = entries_.find(sco);
    if (it != entries_.end())
    {
        SCOEntriesPtr ptr = it->second;
        for (const auto& entries : *ptr)
        {
            for (const auto& e : entries)
            {
                if (((e.cli_.number() == start.number() and
                      e.cli_.offset() >= start.offset()) or
                     (e.cli_.number() > start.number())) and
                    count < max)
                {
                    VERIFY(e.size_ == cluster_size());
                    fun(e.cli_,
                        e.lba_,
                        vec.data(),
                        e.size_);
                    ++count;
                }

                if (count >= max)
                {
                    return count;
                }
            }
        }
    }

    return count;
}

std::ostream&
operator<<(std::ostream& os,
           const NullBackend::Config&)
{
    return os << "NullBackend::Config{}";
}

}

}
