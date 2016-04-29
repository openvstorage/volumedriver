// Copyright 2016 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this memory except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "MemoryBackend.h"

namespace failovercache
{

namespace vd = volumedriver;

MemoryBackend::MemoryBackend(const std::string& nspace,
                             const vd::ClusterSize csize)
    : Backend(nspace,
              csize)
{}

void
MemoryBackend::open(const volumedriver::SCO sco)
{
    LOG_INFO(getNamespace() << ": opening " << sco);

    SCOEntriesPtr ptr = std::make_shared<SCOEntries>();
    const auto res(entries_.emplace(std::make_pair(sco,
                                                   ptr)));
    VERIFY(res.second);
    current_ = ptr;
}

void
MemoryBackend::close()
{
    current_ = nullptr;
}

void
MemoryBackend::add_entries(std::vector<vd::FailOverCacheEntry> entries,
                           std::unique_ptr<uint8_t[]> buf)
{
    VERIFY(current_);

    const size_t cap = current_->capacity();
    const size_t size = current_->size();

    if (size + 1 > cap)
    {
        current_->reserve(cap + sco_entries_slab_size_);
    }

    current_->emplace_back(Entry(std::move(entries),
                                 std::move(buf)));
}

void
MemoryBackend::flush()
{}

void
MemoryBackend::remove(const volumedriver::SCO sco)
{
    LOG_INFO(getNamespace() << ": removing " << sco);

    entries_.erase(sco);
}

void
MemoryBackend::get_entries(const volumedriver::SCO sco,
                           Backend::EntryProcessorFun& fun)
{
    LOG_INFO(getNamespace() << ": processing " << sco);

    const auto it = entries_.find(sco);
    if (it != entries_.end())
    {
        SCOEntriesPtr ptr = it->second;
        for (const auto& entries : *ptr)
        {
            for (const auto& e : entries.first)
            {
                fun(e.cli_,
                    e.lba_,
                    e.buffer_,
                    e.size_);
            }
        }
    }
}

}
