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

namespace failovercache
{

using namespace volumedriver;
using namespace fungi;

namespace be = backend;
namespace fs = boost::filesystem;

Backend::Backend(const std::string& nspace,
                 const ClusterSize cluster_size)
    : registered_(false)
    , first_command_must_be_getEntries(false)
    , ns_(nspace)
    , cluster_size_(cluster_size)
    , check_offset_(0)
{
}

void
Backend::clear_cache_()
{
    LOG_INFO(ns_ << ": clearing");

    for (auto sco : scosdeque_)
    {
        remove(sco);
    }

    close();
    scosdeque_.clear();
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
        SCONumber oldest = scosdeque_.front().number();

        LOG_DEBUG("Oldest SCONUmber in cache " << oldest <<
                  " sconumber passed: " << sconum <<
                  ", namespace: " << ns_);

        if (sconum < oldest)
        {
            LOG_DEBUG("not removing anything for namespace: " << ns_);
            return;
        }

        if (sconame == scosdeque_.back())
        {
            LOG_DEBUG("Closing sco" << sconame);
            close();
        }

        while(not scosdeque_.empty() and
              sconum >= scosdeque_.front().number())
        {
            remove(scosdeque_.front());
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
    const SCOOffset offset = loc.offset();
    const size_t count = entries.size();

    VERIFY(sco == entries.back().cli_.sco());

    if (first_command_must_be_getEntries)
    {
        clear_cache_();
        first_command_must_be_getEntries = false;
    }

    if (scosdeque_.empty() or
        scosdeque_.back() != sco)
    {
        close();

        LOG_INFO(getNamespace() << ": opening new sco: " << sco);
        open(sco);

        scosdeque_.push_back(sco);
        check_offset_ = offset;
    }
    else
    {
        VERIFY(offset == check_offset_);
    }

    add_entries(std::move(entries),
                std::move(buf));

    check_offset_ += count;
}

void
Backend::clear()
{
    LOG_INFO(ns_ << ": clearing");

    clear_cache_();
    first_command_must_be_getEntries = false;
}

void
Backend::getEntries(EntryProcessorFun fun)
{
    LOG_INFO(ns_ << ": getting all entries");

    flush();

    for (const auto& sco : scosdeque_)
    {
        get_entries(sco,
                    fun);
    }

    first_command_must_be_getEntries = false;
}

void
Backend::getSCORange(SCO& oldest,
                     SCO& youngest) const
{
    LOG_DEBUG(ns_);

    if(not scosdeque_.empty())
    {
        oldest= scosdeque_.front();
        youngest = scosdeque_.back();
    }
    else
    {
        oldest = SCO();
        youngest = SCO();
    }
}

void
Backend::getSCO(SCO sconame,
                EntryProcessorFun fun)
{
    LOG_INFO(ns_ << ": getting SCO " << std::hex << sconame);

    const auto it = std::find(scosdeque_.begin(), scosdeque_.end(),sconame);
    if(it == scosdeque_.end())
    {
        return;
    }
    else
    {
        flush();
        get_entries(*it,
                    fun);
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
