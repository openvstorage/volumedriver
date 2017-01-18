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
                            const volumedriver::FailOverCacheTransport transport,
                            const boost::optional<size_t> file_backend_buffer_size = boost::none)
        : path(pth)
        , port(prt)
        , acceptor_(path,
                    file_backend_buffer_size,
                    boost::chrono::microseconds(0))
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
