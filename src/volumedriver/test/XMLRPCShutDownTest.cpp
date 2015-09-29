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

#if 0
#warning "Figure out what to do here once xmlrpc is packaged (all tests seems disabled anyhow)"
#include <sys/types.h>
#include <sys/wait.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "xmlrpc++0.7/src/XmlRpc.h"
#include "xmlrpc++0.7/src/XmlRpcClient.h"
#include "xmlrpc++0.7/src/Server.h"

#include "ExGTest.h"

namespace xmlrpc
{

namespace ba = boost::asio;
namespace vd = volumedriver;
namespace x = XmlRpc;

namespace
{
const std::string shutdown_call("shutdown");
const std::string shutdown_res_key("msg");
const std::string shutdown_res_val("bye");

class ShutDownCall
    : public x::XmlRpcServerMethod
{
public:
    explicit ShutDownCall(xmlrpc::Server& server)
        : x::XmlRpcServerMethod(shutdown_call)
        , server_(server)
    {}

    ShutDownCall(const ShutDownCall&) = delete;

    ShutDownCall&
    operator=(const ShutDownCall&) = delete;

    virtual void
    execute(x::XmlRpcValue& /* params */,
            x::XmlRpcValue& result)
    {
        result[shutdown_res_key] = shutdown_res_val;
        server_.stop();
    }

private:
    xmlrpc::Server& server_;
};

}

class XMLRPCShutDownTest
    : public vd::ExGTest
{
protected:
    XMLRPCShutDownTest()
        : port_(23456)
        , pid_(-1)
    {}

    virtual void
    SetUp()
    {
        pid_ = fork();
        ASSERT_LE(0, pid_) << "failed to fork off server process";
        if (pid_ == 0)
        {
            runServer();
            exit(0);
        }
        else
        {
            waitForServer();
        }
    }

    virtual void
    TearDown()
    {
        if (pid_ > 0)
        {
            int status = 0;
            pid_t ret = waitpid(pid_,
                                &status,
                                0);
            EXPECT_EQ(pid_, ret) << "waitpid returned " << ret;
        }
    }

    void
    waitForServer()
    {
        ba::io_service io_service;
        ba::ip::tcp::socket socket(io_service);
        ba::ip::tcp::endpoint endpoint(ba::ip::address::from_string(host()),
                                       port());

        while (true)
        {
            boost::system::error_code ec;
            socket.connect(endpoint, ec);
            if (ec.value() == ECONNREFUSED)
            {
                boost::this_thread::sleep(boost::posix_time::milliseconds(20));
            }
            else
            {
                EXPECT_EQ(0, ec.value()) <<
                    "failed to connect to xmlrpc server: " <<
                    ec.message();
                break;
            }
        }
    }

    virtual void
    runServer()
    {
        xmlrpc::Server server(port(),
                              256,
                              xmlrpc::ServerType::MT);
        server.addMethod(new ShutDownCall(server));
        server.start();
        server.join();
    }

    uint16_t
    port() const
    {
        return port_;
    }

    const std::string&
    host() const
    {
        static const std::string s("127.0.0.1");
        return s;
    }

private:
    uint16_t port_;
    pid_t pid_;
};

TEST_F(XMLRPCShutDownTest, DISABLED_ditto)
{
    x::XmlRpcClient client(host().c_str(), port());

    x::XmlRpcValue params;
    x::XmlRpcValue result;

    client.execute(shutdown_call.c_str(),
                   params,
                   result);

    EXPECT_FALSE(client.isFault());
    EXPECT_TRUE(result[shutdown_res_key] == shutdown_res_val);
}

namespace
{

MAKE_EXCEPTION(StopNotAllowedException, fungi::IOException);

class StopAfterX
{
public:
    explicit StopAfterX(unsigned x)
        : x_(x)
        , called_(0)
    {}

    void
    operator()()
    {
        if (called_++ < x_)
        {
            throw StopNotAllowedException("stop not allowed");
        }
    }

    unsigned
    x() const
    {
        return x_;
    }

private:
    const unsigned x_;
    unsigned called_;
};

}

class XMLRPCMaybeShutDownTest
    : public XMLRPCShutDownTest
{
protected:
    XMLRPCMaybeShutDownTest()
        : XMLRPCShutDownTest()
        , stop_after_x_(7)
    {}

    virtual void
    runServer()
    {
        xmlrpc::Server server(port(),
                              256,
                              xmlrpc::ServerType::MT);
        server.start();
        server.join();
    }

    StopAfterX stop_after_x_;
};

TEST_F(XMLRPCMaybeShutDownTest, DISABLED_ditto)
{
    x::XmlRpcValue params;
    x::XmlRpcValue result;
    const char* const call = "system.stop";

    for (unsigned i = 0; i < stop_after_x_.x(); ++i)
    {
        x::XmlRpcClient client(host().c_str(), port());
        client.execute(call, params, result);
        EXPECT_TRUE(client.isFault());
        client.close();
    }

    x::XmlRpcClient client(host().c_str(), port());
    client.execute(call, params, result);
    EXPECT_FALSE(client.isFault());
}

}

#endif

// Local Variables: **
// mode: c++ **
// End: **
