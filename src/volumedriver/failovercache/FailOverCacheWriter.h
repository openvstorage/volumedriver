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

#include "fungilib/File.h"

#include <string>
#include <deque>

#include <boost/filesystem.hpp>

#include "../ClusterLocation.h"
#include "../FailOverCacheStreamers.h"
#include "../Types.h"

namespace failovercache
{

class FailOverCacheWriter
{
public:
    FailOverCacheWriter(const boost::filesystem::path& root,
                        const std::string& ns,
                        const volumedriver::ClusterSize& cluster_size);

    ~FailOverCacheWriter();

    void
    addEntries(std::vector<volumedriver::FailOverCacheEntry>,
               std::unique_ptr<uint8_t[]>);

    void
    addEntry(volumedriver::ClusterLocation clusterLocation,
             const uint64_t lba,
             // const would be nice but the funky::WrapByteArray
             // used internally prevents it for now :(
             byte* buf,
             uint32_t size);

    void
    removeUpTo(const volumedriver::SCO);

    using EntryProcessorFun = std::function<void(volumedriver::ClusterLocation,
                                                 int64_t /* addr */,
                                                 const uint8_t* /* buf */,
                                                 int64_t /* size */)>;

    void
    getEntries(EntryProcessorFun);

    void
    getSCO(volumedriver::SCO,
           EntryProcessorFun);

    void
    Clear();

    void
    Flush();

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

private:
    DECLARE_LOGGER("FailOverCacheWriter");

    boost::filesystem::path
    makePath(const volumedriver::SCO) const;

    bool registered_;

    bool first_command_must_be_getEntries;

    const boost::filesystem::path root_;

    const std::string ns_;

    std::deque<volumedriver::SCO> scosdeque_;

    fungi::File* f_;

    const volumedriver::ClusterSize cluster_size_;

    void
    ClearCache();

    int64_t check_offset;
};

}
#endif /* DISKVOLUMEDRIVERCACHE_H_ */

// Local Variables: **
// mode: c++ **
// End: **
