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

#ifndef FAILOVERCACHEPROXY_H
#define FAILOVERCACHEPROXY_H

#include "FailOverCacheStreamers.h"
#include "SCOProcessorInterface.h"
#include "SCO.h"

#include "failovercache/fungilib/Socket.h"
#include "failovercache/fungilib/IOBaseStream.h"
#include "failovercache/fungilib/Buffer.h"

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/scoped_array.hpp>

namespace volumedriver
{

class FailOverCacheConfig;
class Volume;

class FailOverCacheProxy
{
public:
    FailOverCacheProxy(const FailOverCacheConfig&,
                       const Namespace&,
                       const LBASize,
                       const ClusterMultiplier,
                       const boost::chrono::seconds);

    FailOverCacheProxy(const FailOverCacheProxy&) = delete;

    FailOverCacheProxy&
    operator=(const FailOverCacheProxy&) = delete;

    ~FailOverCacheProxy();

    void
    addEntries(std::vector<FailOverCacheEntry> entries);

    // returns the SCO size - 0 indicates a problem.
    // Z42: throw instead!
    uint64_t
    getSCOFromFailOver(SCO a,
                       SCOProcessorFun processor);

    void
    getSCORange(SCO& oldest,
                SCO& youngest);

    void
    clear();

    void
    flush();

    void
    removeUpTo(const SCO sconame) throw ();

    void
    setRequestTimeout(const boost::chrono::seconds);

    void
    getEntries(SCOProcessorFun processor);

    void
    delete_failover_dir()
    {
        LOG_INFO("Setting delete_failover_dir_ to true");
        delete_failover_dir_ = true;
    }

    LBASize
    lba_size() const
    {
        return lba_size_;
    }

    ClusterMultiplier
    cluster_multiplier() const
    {
        return cluster_mult_;
    }

private:
    DECLARE_LOGGER("FailOverCacheProxy");

    void
    register_();

    void
    unregister_();

    void
    checkStreamOK(const std::string& ex);

    uint64_t
    getObject_(SCOProcessorFun processor);

    std::unique_ptr<fungi::Socket> socket_;
    fungi::IOBaseStream stream_;
    const Namespace ns_;
    const LBASize lba_size_;
    const ClusterMultiplier cluster_mult_;
    bool delete_failover_dir_;
};

}

#endif // FAILOVERCACHEPROXY_H

// Local Variables: **
// mode: c++ **
// End: **
