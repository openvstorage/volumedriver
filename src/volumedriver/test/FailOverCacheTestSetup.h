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

#ifndef FAILOVER_CACHE_TEST_SETUP_H_
#define FAILOVER_CACHE_TEST_SETUP_H_

#include <memory>
#include <set>

#include <boost/filesystem.hpp>

#include <youtils/Logging.h>

#include "../FailOverCacheConfig.h"
#include "../FailOverCacheProxy.h"
#include "../FailOverCacheTransport.h"
#include "../failovercache/FailOverCacheAcceptor.h"
#include "../failovercache/FailOverCacheProtocol.h"

class VolumeDriverTest;

namespace volumedrivertest
{

class FailOverCacheTestSetup;

class FailOverCacheTestContext
{
private:
    friend class FailOverCacheTestSetup;

    FailOverCacheTestContext(FailOverCacheTestSetup& setup,
                             const boost::optional<std::string>& addr,
                             const uint16_t port);

    FailOverCacheTestContext(const FailOverCacheTestContext&) = delete;

    FailOverCacheTestContext&
    operator=(const FailOverCacheTestContext&) = delete;

    FailOverCacheTestSetup& setup_;
    const boost::optional<std::string> addr_;
    const uint16_t port_;
    failovercache::FailOverCacheAcceptor acceptor_;
    std::unique_ptr<fungi::SocketServer> server_;

public:
    ~FailOverCacheTestContext();

    boost::filesystem::path
    path() const
    {
        return acceptor_.path();
    }

    boost::filesystem::path
    lock_file() const
    {
        return acceptor_.lock_file();
    }

    uint16_t
    port() const
    {
        return port_;
    }

    volumedriver::FailOverCacheConfig
    config() const;
};

typedef std::shared_ptr<FailOverCacheTestContext> foctest_context_ptr;

class FailOverCacheTestSetup
{
    friend class FailOverCacheTestContext;
    friend class ::VolumeDriverTest;

public:
    explicit FailOverCacheTestSetup(const boost::filesystem::path& p);

    ~FailOverCacheTestSetup();

    FailOverCacheTestSetup(const FailOverCacheTestSetup&) = delete;

    FailOverCacheTestSetup&
    operator=(const FailOverCacheTestSetup&) = delete;

    foctest_context_ptr
    start_one_foc();

    static uint16_t
    port_base()
    {
        return port_base_;
    }

    uint16_t
    get_next_foc_port() const;

    static std::string
    host()
    {
        return addr_;
    }

    static volumedriver::FailOverCacheTransport
    transport()
    {
        return transport_;
    }

    const boost::filesystem::path path;

private:
    DECLARE_LOGGER("FailOverCacheTestSetup");

    static std::string addr_;
    static uint16_t port_base_;
    static volumedriver::FailOverCacheTransport transport_;

    typedef std::set<uint16_t> set_type;
    set_type ports_;

    void
    release_port_(uint16_t port)
    {
        ports_.erase(port);
    }
};

}

#endif //!FAILOVER_CACHE_TEST_SETUP_H_

// Local Variables: **
// mode: c++ **
// End: **
