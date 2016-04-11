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

#ifndef FAILOVERCACHEWRITER_H_
#define FAILOVERCACHEWRITER_H_

#include <string>
#include <deque>

#include "../ClusterLocation.h"
#include "../FailOverCacheStreamers.h"
#include "../Types.h"

#include <boost/filesystem.hpp>

namespace failovercache
{

class FailOverCacheWriter
{
public:
    static std::unique_ptr<FailOverCacheWriter>
    create(const boost::filesystem::path&,
           const std::string&,
           const volumedriver::ClusterSize);

    using EntryProcessorFun = std::function<void(volumedriver::ClusterLocation,
                                                 int64_t /* addr */,
                                                 const uint8_t* /* buf */,
                                                 int64_t /* size */)>;

    virtual ~FailOverCacheWriter() = default;

    FailOverCacheWriter(const FailOverCacheWriter&) = delete;

    FailOverCacheWriter&
    operator=(const FailOverCacheWriter&) = delete;

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
    FailOverCacheWriter(const std::string&,
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
    DECLARE_LOGGER("FailOverCacheWriter");

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
#endif /* DISKVOLUMEDRIVERCACHE_H_ */

// Local Variables: **
// mode: c++ **
// End: **
