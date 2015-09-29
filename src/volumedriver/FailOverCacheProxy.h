// Copyright 2015 Open vStorage NV
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

#include "failovercache/fungilib/Socket.h"
#include "failovercache/fungilib/IOBaseStream.h"
#include "failovercache/fungilib/Buffer.h"
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/scoped_array.hpp>
#include "SCO.h"

namespace volumedriver
{

class FailOverCacheConfig;
class Volume;

class FailOverCacheProxy
{
public:
    FailOverCacheProxy(const FailOverCacheConfig& cfg,
                       const Namespace& nameSpace,
                       const int32_t clustersize,
                       unsigned timeout);

    FailOverCacheProxy(const FailOverCacheProxy&) = delete;
    FailOverCacheProxy& operator=(const FailOverCacheProxy&) = delete;

    ~FailOverCacheProxy();

    void
    addEntries(boost::ptr_vector<FailOverCacheEntry>& entries);

    // returns the SCO size - 0 indicates a problem.
    // Z42: throw instead!
    template<typename T>
    uint64_t
    getSCOFromFailOver(SCO a,
                       T& processor)
    {
        *stream_ << fungi::IOBaseStream::cork;
        OUT_ENUM(*stream_, GetSCO);
        *stream_ << a;
        *stream_ << fungi::IOBaseStream::uncork;
        return getObject_<T>(processor);
    }

    void
    getSCORange(SCO& oldest,
                SCO& youngest);

    void
    clear();

    void
    flush();

    void
    removeUpTo(const SCO sconame) throw ();

    DECLARE_LOGGER("FailOverCacheProxy");

    void
    setRequestTimeout(const uint32_t seconds);

    template<typename T>
    void
    getEntries(T& processor)
    {
        // Y42 review later
        *stream_ << fungi::IOBaseStream::cork;
        OUT_ENUM(*stream_, GetEntries);
        *stream_ << fungi::IOBaseStream::uncork;

        getObject_<T>(processor);
    }

    void
    delete_failover_dir()
    {
        LOG_INFO("Setting delete_failover_dir_ to true");
        delete_failover_dir_ = true;
    }

private:
    void
    Register_();

    void
    Unregister_();

    void
    checkStreamOK(const std::string& ex);

    template<typename T>
    uint64_t
    getObject_(T& processor)
    {
        fungi::Buffer buf;
        *stream_ >> fungi::IOBaseStream::cork;
        uint64_t ret = 0;
        while (true)
        {
            ClusterLocation cli;
            *stream_ >> cli;
            if(cli.isNull())
            {
                return ret;
            }
            else
            {
                uint64_t lba = 0;
                *stream_ >> lba;

                int64_t bal; // byte array length
                *stream_ >> bal;

                int32_t size = (int32_t) bal;
                buf.store(stream_->getSink(), size);
                processor(cli, lba, buf.data(), size);
                ret += size;
            }
        }
    }

    fungi::Socket* socket_;
    fungi :: IOBaseStream *stream_;
    const Namespace ns_;
    const ClusterSize clustersize_;
    bool delete_failover_dir_;
};

}

#endif // FAILOVERCACHEPROXY_H

// Local Variables: **
// mode: c++ **
// End: **
