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
#ifndef DTL_MEMORY_BACKEND_H_
#define DTL_MEMORY_BACKEND_H_

#include "Backend.h"

#include <memory>
#include <unordered_map>

namespace distributed_transaction_log
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
