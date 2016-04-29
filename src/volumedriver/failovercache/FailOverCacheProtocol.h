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

#ifndef FAILOVERCACHEPROTOCOL_H
#define FAILOVERCACHEPROTOCOL_H

#include "fungilib/Protocol.h"
#include "fungilib/Socket.h"
#include "fungilib/SocketServer.h"
#include "fungilib/Thread.h"
#include "fungilib/IOBaseStream.h"
#include <youtils/IOException.h>
#include "../Types.h"
#include "../ClusterLocation.h"

namespace failovercache
{
class FailOverCacheAcceptor;
class Backend;

class FailOverCacheProtocol
    : public fungi::Protocol
{
public:
    FailOverCacheProtocol(std::unique_ptr<fungi::Socket> sock,
                          fungi::SocketServer& /*parentServer*/,
                          FailOverCacheAcceptor& fact);

    ~FailOverCacheProtocol();

    virtual void
    start() override final;

    virtual void
    run() override final;

    void
    stop();

    virtual const char*
    getName() const override final
    {
        return "FailOverCacheProtocol";
    };

private:
    DECLARE_LOGGER("FailOverCacheProtocol");

    std::shared_ptr<Backend> cache_;
    std::unique_ptr<fungi::Socket> sock_;
    fungi::IOBaseStream stream_;
    fungi::Thread* thread_;

    FailOverCacheAcceptor& fact_;
    bool use_rs_;

    int pipes_[2];
    int nfds_;

    void
    addEntries_();

    void
    getEntries_();

    void
    Flush_();

    void
    register_();

    void
    unregister_();

    void
    getSCO_();

    void
    getSCORange_();

    void
    Clear_();

    void
    returnOk();

    void
    returnNotOk();

    void
    removeUpTo_();

    void
    processFailOverCacheEntry_(volumedriver::ClusterLocation cli,
                               int64_t lba,
                               const byte* buf,
                               int64_t size);
};

}


#endif // FAILOVERCACHEPROTOCOL_H

// Local Variables: **
// mode: c++ **
// End: **
