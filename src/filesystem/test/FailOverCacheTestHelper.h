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

#ifndef VFSTEST_FAILOVERCACHE_TEST_HELPER_H_
#define VFSTEST_FAILOVERCACHE_TEST_HELPER_H_

#include <boost/filesystem/path.hpp>

#include <volumedriver/FailOverCacheTransport.h>
#include <volumedriver/failovercache/FailOverCacheAcceptor.h>
#include <volumedriver/failovercache/FailOverCacheProtocol.h>

namespace volumedriverfstest
{

class FailOverCacheTestHelper
{
public:
    FailOverCacheTestHelper(const boost::filesystem::path& pth,
                            const boost::optional<std::string>& addr,
                            const uint16_t prt,
                            const volumedriver::FailOverCacheTransport transport)
        : path(pth)
        , port(prt)
        , acceptor_(path)
        , server_(fungi::SocketServer::createSocketServer(acceptor_,
                                                          addr,
                                                          port,
                                                          transport == volumedriver::FailOverCacheTransport::RSocket))
    {}

    ~FailOverCacheTestHelper()
    {
        server_->stop();
        server_->join();
        server_.reset();
    }

    FailOverCacheTestHelper(const FailOverCacheTestHelper&) = delete;

    FailOverCacheTestHelper&
    operator=(const FailOverCacheTestHelper&) = delete;

    const boost::filesystem::path path;
    const uint16_t port;

private:
    failovercache::FailOverCacheAcceptor acceptor_;
    std::unique_ptr<fungi::SocketServer> server_;
};

}

#endif //! VFSTEST_FAILOVERCACHE_TEST_HELPER_H_
