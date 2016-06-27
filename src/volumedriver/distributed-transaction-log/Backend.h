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

#ifndef DTL_BACKEND_H_
#define DTL_BACKEND_H_

#include <string>
#include <deque>

#include "../ClusterLocation.h"
#include "../DtlStreamers.h"
#include "../Types.h"

namespace distributed_transaction_log
{

class Backend
{
public:
    using EntryProcessorFun = std::function<void(volumedriver::ClusterLocation,
                                                 int64_t /* addr */,
                                                 const uint8_t* /* buf */,
                                                 int64_t /* size */)>;

    virtual ~Backend() = default;

    Backend(const Backend&) = delete;

    Backend&
    operator=(const Backend&) = delete;

    void
    addEntries(std::vector<volumedriver::FailOverCacheEntry>,
               std::unique_ptr<uint8_t[]>);

    void
    removeUpTo(const volumedriver::SCO);

    void
    getEntries(EntryProcessorFun);

    void
    getSCO(volumedriver::SCO,
           EntryProcessorFun);

    void
    clear();

    void
    register_()
    {
        LOG_DEBUG("Registering for namespace " << ns_);
        registered_ = true;
    }

    bool
    registered() const
    {
        return registered_;
    }

    const std::string&
    getNamespace() const
    {
        return ns_;
    }

    void
    setFirstCommandMustBeGetEntries()
    {
        first_command_must_be_getEntries = true;
    }

    void
    getSCORange(volumedriver::SCO& oldest,
                volumedriver::SCO& youngest) const;

    volumedriver::ClusterSize
    cluster_size() const
    {
        return cluster_size_;
    }

    virtual void
    flush() = 0;

protected:
    Backend(const std::string&,
            const volumedriver::ClusterSize);

    virtual void
    open(const volumedriver::SCO) = 0;

    virtual void
    close() = 0;

    virtual void
    add_entries(std::vector<volumedriver::FailOverCacheEntry>,
                std::unique_ptr<uint8_t[]>) = 0;

    virtual void
    get_entries(const volumedriver::SCO,
                EntryProcessorFun&) = 0;

    virtual void
    remove(const volumedriver::SCO) = 0;

private:
    DECLARE_LOGGER("Backend");

    bool registered_;
    bool first_command_must_be_getEntries;
    const std::string ns_;
    std::deque<volumedriver::SCO> scosdeque_;
    const volumedriver::ClusterSize cluster_size_;

    void
    clear_cache_();

    size_t check_offset_;
};

}
#endif // !DTL_BACKEND_H_

// Local Variables: **
// mode: c++ **
// End: **
