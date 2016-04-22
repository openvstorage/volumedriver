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

#ifndef FAILOVERCACHEACCEPTOR_H
#define FAILOVERCACHEACCEPTOR_H

#include "FailOverCacheProtocol.h"
#include "BackendFactory.h"

#include "../FailOverCacheStreamers.h"

#include <unordered_map>

#include <boost/thread/mutex.hpp>

#include "fungilib/ProtocolFactory.h"

namespace volumedrivertest
{

class FailOverCacheTestContext;

}

namespace failovercache
{

class FailOverCacheAcceptor
    : public fungi::ProtocolFactory
{
    friend class volumedrivertest::FailOverCacheTestContext;

public:
    explicit FailOverCacheAcceptor(const boost::optional<boost::filesystem::path>& root);

    virtual ~FailOverCacheAcceptor();

    virtual fungi::Protocol*
    createProtocol(std::unique_ptr<fungi::Socket>,
                   fungi::SocketServer& parentServer) override final;

    virtual const char *
    getName() const override final
    {
        return "FailOverCacheAcceptor";
    }

    void
    remove(Backend&);

    using BackendPtr = std::shared_ptr<Backend>;

    BackendPtr
    lookup(const volumedriver::CommandData<volumedriver::Register>&);

    void
    removeProtocol(FailOverCacheProtocol* prot)
    {
        boost::lock_guard<decltype(mutex_)> g(mutex_);
        protocols.remove(prot);
    }

private:
    DECLARE_LOGGER("FailOverCacheAcceptor");

    // protects the map / protocols.
    boost::mutex mutex_;

    using Map = std::unordered_map<std::string, BackendPtr>;
    Map map_;

    std::list<FailOverCacheProtocol*> protocols;
    BackendFactory factory_;

    // for use by testers
    BackendPtr
    find_backend_(const std::string&);
};

}
#endif // FAILOVERCACHEACCEPTOR_H

// Local Variables: **
// mode : c++ **
// End: **
