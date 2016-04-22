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

#ifndef DTL_MEMORY_BACKEND_H_
#define DTL_MEMORY_BACKEND_H_

#include "Backend.h"

#include <memory>
#include <unordered_map>

namespace failovercache
{

class MemoryBackend
    : public Backend
{
public:
    MemoryBackend(const std::string&,
                  const volumedriver::ClusterSize);

    ~MemoryBackend() = default;

    MemoryBackend(const MemoryBackend&) = delete;

    MemoryBackend&
    operator=(const MemoryBackend&) = delete;

    virtual void
    open(const volumedriver::SCO) override final;

    virtual void
    close() override final;

    virtual void
    add_entries(std::vector<volumedriver::FailOverCacheEntry>,
                std::unique_ptr<uint8_t[]>) override final;

    virtual void
    flush() override final;

    virtual void
    remove(const volumedriver::SCO) override final;

    virtual void
    get_entries(const volumedriver::SCO,
                Backend::EntryProcessorFun&) override final;

private:
    DECLARE_LOGGER("DtlMemoryBackend");

    using Entry = std::pair<std::vector<volumedriver::FailOverCacheEntry>,
                            std::unique_ptr<uint8_t[]>>;

    using SCOEntries = std::vector<Entry>;
    using SCOEntriesPtr = std::shared_ptr<SCOEntries>;

    SCOEntriesPtr current_;

    struct Hash
    {
        size_t
        operator()(const volumedriver::SCO sco) const
        {
            return sco.number();
        }
    };

    std::unordered_map<volumedriver::SCO,
                       SCOEntriesPtr,
                       Hash> entries_;

    // capacity / increment used when adding to SCOEntries
    static constexpr size_t sco_entries_slab_size_ = 4096;
};

}

#endif // !DTL_MEMORY_BACKEND_H_
