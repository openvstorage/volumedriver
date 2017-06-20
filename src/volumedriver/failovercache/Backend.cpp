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

#include "Backend.h"

#include "../FailOverCacheEntry.h"

#include <youtils/Assert.h>

namespace volumedriver
{

namespace failovercache
{

using namespace fungi;

namespace be = backend;
namespace fs = boost::filesystem;

Backend::Backend(const std::string& nspace,
                 const ClusterSize cluster_size)
    : registered_(false)
    , first_command_must_be_getEntries(false)
    , ns_(nspace)
    , last_loc_(0)
    , cluster_size_(cluster_size)
{
}

void
Backend::clear_cache_()
{
    LOG_INFO(ns_ << ": clearing");

    for (auto loc : scosdeque_)
    {
        remove(loc.sco());
    }

    close();
    scosdeque_.clear();
    last_loc_ = ClusterLocation(0);
}

void
Backend::removeUpTo(const SCO sconame)
{
    LOG_INFO(ns_ << ": removing up to " << sconame);

    VERIFY(sconame.version() == 0);
    VERIFY(sconame.cloneID() == 0);

    if(!scosdeque_.empty())
    {
        SCONumber sconum = sconame.number();
        SCONumber oldest = scosdeque_.front().sco().number();

        LOG_DEBUG("Oldest SCONUmber in cache " << oldest <<
                  " sconumber passed: " << sconum <<
                  ", namespace: " << ns_);

        if (sconum < oldest)
        {
            LOG_DEBUG("not removing anything for namespace: " << ns_);
            return;
        }

        if (sconame == scosdeque_.back().sco())
        {
            LOG_DEBUG("Closing sco" << sconame);
            close();
            last_loc_ = ClusterLocation(0);
        }

        while(not scosdeque_.empty() and
              sconum >= scosdeque_.front().number())
        {
            remove(scosdeque_.front().sco());
            scosdeque_.pop_front();
        }
    }
}

void
Backend::addEntries(std::vector<FailOverCacheEntry> entries,
                    std::unique_ptr<uint8_t[]> buf)
{
    VERIFY(not entries.empty());

    const ClusterLocation& loc = entries.front().cli_;
    const SCO sco = loc.sco();
    const size_t count = entries.size();

    VERIFY(sco == entries.back().cli_.sco());

    if (first_command_must_be_getEntries)
    {
        LOG_INFO(ns_ << ": first command must be get_entries, clearing!");
        clear_cache_();
        first_command_must_be_getEntries = false;
    }

    if (scosdeque_.empty() or
        scosdeque_.back().sco() != sco)
    {
        close();

        LOG_INFO(getNamespace() << ": opening new sco: " << sco  << " for " << loc);
        open(sco);

        scosdeque_.push_back(loc);
    }
    else
    {
        VERIFY(last_loc_.offset() + 1 == loc.offset());
    }

    last_loc_ = loc;
    last_loc_.offset(last_loc_.offset() + count - 1);

    add_entries(std::move(entries),
                std::move(buf));
}

void
Backend::clear()
{
    LOG_INFO(ns_ << ": clearing");

    clear_cache_();
    first_command_must_be_getEntries = false;
}

size_t
Backend::get_entries(ClusterLocation start,
                     size_t max,
                     EntryProcessorFun fun)
{
    LOG_INFO(ns_ << ": [" << start << ", +" << max << ")");

    flush();

    size_t count = 0;
    if (start == ClusterLocation(0) and not scosdeque_.empty())
    {
        start = scosdeque_.front();
    }

    for (const auto& loc : scosdeque_)
    {
        if (loc.sco() < start.sco())
        {
            continue;
        }

        SCOOffset off = 0;

        if (loc.sco() == start.sco() and
            start.offset() >= loc.offset())
        {
            off = start.offset() - loc.offset();
        }

        count += get_entries(loc.sco(),
                             off,
                             max - count,
                             fun);

        start = loc;
        if (count >= max)
        {
            break;
        }
    }

    return count;
}

void
Backend::getEntries(EntryProcessorFun fun)
{
    LOG_INFO(ns_ << ": getting all entries");
    get_entries(ClusterLocation(0),
                std::numeric_limits<size_t>::max(),
                std::move(fun));
    first_command_must_be_getEntries = false;
}

std::pair<ClusterLocation, ClusterLocation>
Backend::range() const
{
    ClusterLocation oldest(0);
    ClusterLocation youngest(0);

    if (not scosdeque_.empty())
    {
        VERIFY(last_loc_ != ClusterLocation(0));
        oldest = scosdeque_.front();
        youngest = last_loc_;
    }
    else
    {
        VERIFY(last_loc_ == ClusterLocation(0));
    }

    LOG_INFO(ns_ << ": [" << oldest << ", " << youngest << "]");

    return std::make_pair(oldest,
                          youngest);
}

void
Backend::getSCO(SCO sconame,
                EntryProcessorFun fun)
{
    LOG_INFO(ns_ << ": getting SCO " << std::hex << sconame);

    const auto it = std::find(scosdeque_.begin(),
                              scosdeque_.end(),
                              ClusterLocation(sconame, 0));
    if(it == scosdeque_.end())
    {
        return;
    }
    else
    {
        flush();
        get_entries(it->sco(),
                    it->offset(),
                    std::numeric_limits<size_t>::max(),
                    fun);
    }
}

}

}

// Local Variables: **
// mode: c++ **
// End: **
