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
#include "../FailOverCacheCommand.h"
#include "../OwnerTag.h"
#include "../Types.h"

namespace volumedriver
{

class FailOverCacheEntry;

namespace failovercache
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
               std::unique_ptr<uint8_t[]>,
               const OwnerTag);

    void
    removeUpTo(const SCO,
               const OwnerTag);

    void
    getEntries(EntryProcessorFun);

    size_t
    get_entries(ClusterLocation,
                size_t max,
                EntryProcessorFun);

    void
    getSCO(volumedriver::SCO,
           EntryProcessorFun);

    void
    clear(const OwnerTag);

    void
    register_(const OwnerTag owner_tag)
    {
        LOG_INFO(ns_ << ": owner tag " << owner_tag);
        owner_tag_ = owner_tag;
        registered_ = true;
    }

    bool
    registered() const
    {
        return registered_;
    }

    OwnerTag
    owner_tag() const
    {
        return owner_tag_;
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

    std::pair<ClusterLocation, ClusterLocation>
    range() const;

    volumedriver::ClusterSize
    cluster_size() const
    {
        return cluster_size_;
    }

    void
    flush(const OwnerTag owner_tag)
    {
        check_owner_(owner_tag);
        flush();
    }

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

    virtual size_t
    get_entries(const volumedriver::SCO,
                const SCOOffset,
                size_t max,
                EntryProcessorFun&) = 0;

    virtual void
    remove(const volumedriver::SCO) = 0;

    virtual void
    flush() = 0;

private:
    DECLARE_LOGGER("Backend");

    OwnerTag owner_tag_;
    bool registered_;
    bool first_command_must_be_getEntries;
    const std::string ns_;
    std::deque<ClusterLocation> scosdeque_;
    ClusterLocation last_loc_;
    const volumedriver::ClusterSize cluster_size_;

    void
    clear_cache_();

    void
    check_owner_(const OwnerTag) const;
};

}

}

#endif // !DTL_BACKEND_H_

// Local Variables: **
// mode: c++ **
// End: **
