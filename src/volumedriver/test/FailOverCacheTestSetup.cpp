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

#include "FailOverCacheTestSetup.h"

#include <boost/lexical_cast.hpp>

#include <youtils/System.h>
#include <gtest/gtest.h>

namespace volumedrivertest
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace vd = volumedriver;

using namespace std::literals::string_literals;

namespace
{

boost::optional<fs::path>
make_directory(const boost::optional<fs::path>& path,
               uint16_t port)
{
    if (path)
    {
        const fs::path p(path->string() + "-"s + boost::lexical_cast<std::string>(port));
        fs::create_directories(p);
        return p;
    }
    else
    {
        return path;
    }
}

}

FailOverCacheTestContext::FailOverCacheTestContext(FailOverCacheTestSetup& setup,
                                                   const boost::optional<std::string>& addr,
                                                   const uint16_t port,
                                                   const boost::chrono::microseconds busy_retry_duration)
    : setup_(setup)
    , addr_(addr)
    , port_(port)
    , acceptor_(make_directory(setup_.path,
                               port_),
                busy_retry_duration)
    , server_(fungi::SocketServer::createSocketServer(acceptor_,
                                                      addr_,
                                                      port_,
                                                      setup.transport() == vd::FailOverCacheTransport::RSocket))
{}

FailOverCacheTestContext::~FailOverCacheTestContext()
{
    EXPECT_NO_THROW(server_->stop());
    EXPECT_NO_THROW(server_->join());
    server_.reset();
    setup_.release_port_(port_);
}

vd::FailOverCacheConfig
FailOverCacheTestContext::config(const vd::FailOverCacheMode mode) const
{
    return vd::FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                   port(),
                                   mode);
}

std::shared_ptr<failovercache::Backend>
FailOverCacheTestContext::backend(const be::Namespace& nspace)
{
    return acceptor_.find_backend_(nspace.str());
}

std::string
FailOverCacheTestSetup::addr_("127.0.0.1");

uint16_t
FailOverCacheTestSetup::port_base_ =
    youtils::System::get_env_with_default<uint16_t>("FOC_PORT_BASE", 17071);

vd::FailOverCacheTransport
FailOverCacheTestSetup::transport_(vd::FailOverCacheTransport::TCP);

boost::chrono::microseconds
FailOverCacheTestSetup::busy_retry_duration_(0);

FailOverCacheTestSetup::FailOverCacheTestSetup(const boost::optional<fs::path>& p)
        : path(p)
{
    if (path)
    {
        fs::create_directories(*path);
    }
    LOG_INFO("path " << path << ", port base " << port_base_);
}

FailOverCacheTestSetup::~FailOverCacheTestSetup()
{
    EXPECT_TRUE(ports_.empty()) << "there are still ports in use - prepare to crash";
    if (path)
    {
        EXPECT_NO_THROW(fs::remove_all(*path));
    }
}

uint16_t
FailOverCacheTestSetup::get_next_foc_port() const
{
    uint16_t port = port_base_;

    const auto it = ports_.crbegin();
    if (it != ports_.crend())
    {
        port = *it + 1;
    }
    return port;

}

foctest_context_ptr
FailOverCacheTestSetup::start_one_foc()
{
    uint16_t port = port_base_;

    auto it = ports_.crbegin();
    if (it != ports_.crend())
    {
        port = *it + 1;
    }

    boost::optional<std::string> addr;
    if (not addr_.empty())
    {
        addr = addr_;
    }

    foctest_context_ptr ctx(new FailOverCacheTestContext(*this,
                                                         addr,
                                                         port,
                                                         busy_retry_duration_));
    ports_.insert(port);

    return ctx;
}

}

// Local Variables: **
// mode: c++ **
// End: **
