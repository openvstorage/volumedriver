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
#include "fungilib/ByteArray.h"
#include "fungilib/Mutex.h"
#include <string>
#include <map>
#include <deque>
#include "fungilib/File.h"
#include <boost/filesystem.hpp>
#include "../Types.h"
#include "../ClusterLocation.h"
namespace failovercache
{
class FailOverCacheProtocol;

namespace fs = boost::filesystem;

class FailOverCacheWriter
{
public:
    FailOverCacheWriter(const fs::path& root,
                        const std::string& ns,
                        const volumedriver::ClusterSize& cluster_size);

    ~FailOverCacheWriter();

    void addEntry(volumedriver::ClusterLocation clusterLocation,
                  const uint64_t lba,
                  // const would be nice but the funky::WrapByteArray
                  // used internally prevents it for now :(
                  byte* buf,
                  uint32_t size);

    void removeUpTo(volumedriver::SCONumber& sco);

    typedef void (*FailOverProcessor)(volumedriver::ClusterLocation cl,
                                      const uint64_t lba,
                                      byte* buf);


    void getEntries(FailOverCacheProtocol* failover_prot) const;

    void
    getSCO(volumedriver::SCO a,
           FailOverCacheProtocol* failover_prot) const;


    void
    Clear();

    void
    Flush();

    DECLARE_LOGGER("FailOverCacheWriter");

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
    removeUpTo(const volumedriver::SCO);

    void
    getSCORange(volumedriver::SCO& oldest,
                volumedriver::SCO& youngest) const;

private:

    fs::path
    makePath(const volumedriver::SCO) const;

    bool registered_;

    mutable bool first_command_must_be_getEntries;

    const fs::path root_;

    const std::string ns_;

    fs::path relFilePath_;

    std::deque<volumedriver::SCO> scosdeque_;
    typedef std::deque<volumedriver::SCO>::iterator sco_iterator;
    typedef std::deque<volumedriver::SCO>::const_iterator sco_const_iterator;

    fungi::File* f_;

    const volumedriver::ClusterSize cluster_size_;

    void
    ClearCache();

    int64_t check_offset;
};

}
#endif /* DISKVOLUMEDRIVERCACHE_H_ */

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
