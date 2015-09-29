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

#include "FailOverCacheTestSetup.h"

#include <boost/lexical_cast.hpp>

#include <youtils/System.h>
#include <youtils/TestBase.h>

namespace volumedrivertest
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;

using namespace std::literals::string_literals;

namespace
{

fs::path
make_directory(const fs::path& path, uint16_t port)
{
    const fs::path p(path.string() + "-"s + boost::lexical_cast<std::string>(port));
    fs::create_directories(p);

    return p;
}

}

FailOverCacheTestContext::FailOverCacheTestContext(FailOverCacheTestSetup& setup,
                                                   const boost::optional<std::string>& addr,
                                                   const uint16_t port)
    : setup_(setup)
    , addr_(addr)
    , port_(port)
    , acceptor_(make_directory(setup_.path,
                               port_))
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
FailOverCacheTestContext::config() const
{
    return vd::FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                   port());
}

std::string
FailOverCacheTestSetup::addr_("127.0.0.1");

uint16_t
FailOverCacheTestSetup::port_base_ =
    youtils::System::get_env_with_default<uint16_t>("FOC_PORT_BASE", 17071);

vd::FailOverCacheTransport
FailOverCacheTestSetup::transport_(vd::FailOverCacheTransport::TCP);

FailOverCacheTestSetup::FailOverCacheTestSetup(const fs::path& p)
        : path(p)
{
    fs::create_directories(path);
    LOG_INFO("path " << path << ", port base " << port_base_);
}

FailOverCacheTestSetup::~FailOverCacheTestSetup()
{
    EXPECT_TRUE(ports_.empty()) << "there are still ports in use - prepare to crash";
    EXPECT_NO_THROW(fs::remove_all(path));
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
                                                         port));
    ports_.insert(port);

    return ctx;
}

}

// Local Variables: **
// mode: c++ **
// End: **
