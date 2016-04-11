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

#include "FailOverCacheWriter.h"
#include "FileBackend.h"

#include <youtils/Assert.h>

namespace failovercache
{

using namespace volumedriver;
using namespace fungi;

namespace be = backend;
namespace fs = boost::filesystem;

FailOverCacheWriter::FailOverCacheWriter(const std::string& nspace,
                                         const ClusterSize cluster_size)
    : registered_(false)
    , first_command_must_be_getEntries(false)
    , ns_(nspace)
    , cluster_size_(cluster_size)
    , check_offset_(0)
{
}

void
FailOverCacheWriter::clear_cache_()
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
FailOverCacheWriter::removeUpTo(const SCO sconame)
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
FailOverCacheWriter::addEntries(std::vector<FailOverCacheEntry> entries,
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
FailOverCacheWriter::clear()
{
    LOG_INFO(ns_ << ": clearing");

    clear_cache_();
    first_command_must_be_getEntries = false;
}

void
FailOverCacheWriter::getEntries(EntryProcessorFun fun)
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
FailOverCacheWriter::getSCORange(SCO& oldest,
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
FailOverCacheWriter::getSCO(SCO sconame,
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

std::unique_ptr<FailOverCacheWriter>
FailOverCacheWriter::create(const fs::path& root,
                            const std::string& nspace,
                            const ClusterSize csize)
{
    return std::make_unique<FileBackend>(root,
                                         nspace,
                                         csize);
}

}

// Local Variables: **
// mode: c++ **
// End: **
