#ifndef XMLRPCTEST_H_
#define XMLRPCTEST_H_
#include "../src/Server.h"
#include "../src/XmlRpcClient.h"
#include <gtest/gtest.h>
#include <youtils/IOException.h>
#include <stdexcept>
#include <boost/filesystem.hpp>
#include <boost/scope_exit.hpp>
#include "../src/XmlRpcValue.h"
#include <boost/thread.hpp>
#include <boost/foreach.hpp>

namespace xmlrpctest
{

class TestConfig
{};

class ThreadedXMLRCPCall
{
public:
    ThreadedXMLRCPCall(const std::string& call,
                       uint16_t port,
                       XmlRpc::XmlRpcValue params = XmlRpc::XmlRpcValue(),
                       double timeout_in_seconds = -1.0)

        : call_(call)
        , port_(port)
        , params_(params)
        , timeout_in_seconds_(timeout_in_seconds)
    {}

    ThreadedXMLRCPCall(const ThreadedXMLRCPCall&) = delete;
    ThreadedXMLRCPCall&
    operator=(const ThreadedXMLRCPCall&) = delete;

    // ThreadedXMLRCPCall&
    // operator=(ThreadedXMLRCPCall&& in)
    // {
    //     call_ = std::move(in.call_);
    //     port_ = in.port_;
    //     params_ = in.params_;
    //     timeout_in_seconds_ = in.timeout_in_seconds_;
    //     return *this;
    // }


    // ThreadedXMLRCPCall(ThreadedXMLRCPCall&& in)
    //     : call_(std::move(in.call_))
    //     , port_(in.port_)
    //     , params_(in.params_)
    //     , timeout_in_seconds_(in.timeout_in_seconds_)
    // {}


    ~ThreadedXMLRCPCall()
    {
        if(thread_)
        {
            thread_->join();
        }
    }

    void
    start(double timeout_in_seconds = -1.0)
    {
        timeout_in_seconds_ = timeout_in_seconds;

        thread_.reset(new boost::thread(boost::ref(*this)));
    }

    void
    join()
    {
        thread_->join();
    }

    template<typename TimeDuration>
    bool
    timed_join(TimeDuration dur)
    {
        return thread_->timed_join(dur);
    }

    bool
    finished()
    {
        return thread_->timed_join(boost::posix_time::milliseconds(1));
    }

    bool
    fault() const
    {
        return fault_;
    }

    bool
    clientFault() const
    {
        return clientFault_;

    }

    void
    fault(bool in)
    {
        fault_ = in;
    }

    void
    operator()()
    {
        XmlRpc::XmlRpcClient client("localhost", port_);
        BOOST_SCOPE_EXIT((&client))
        {
            client.close();
        }
        BOOST_SCOPE_EXIT_END;

        XmlRpc::XmlRpcValue params;
        XmlRpc::XmlRpcValue result;
        fault_ = not client.execute(call_.c_str(),
                                    params_,
                                    result_,
                                    timeout_in_seconds_);
        clientFault_ = client.isFault();
        LOG_INFO("EXITERING XMLRPC CALL" << call_);
    }
    DECLARE_LOGGER("ThreadedXMLRCPCall");

    const XmlRpc::XmlRpcValue&
    result() const
    {
        return result_;
    }

    XmlRpc::XmlRpcValue&
    params()
    {
        return params_;
    }

private:
    std::string call_;

    uint16_t port_;
    XmlRpc::XmlRpcValue params_;

    XmlRpc::XmlRpcValue result_;
    bool fault_;
    bool clientFault_;

    std::unique_ptr<boost::thread> thread_;
    double timeout_in_seconds_;

};


class XMLRPCTest : public testing::TestWithParam<TestConfig>
{
public:
    typedef xmlrpc::Server server_t;
    typedef std::unique_ptr<server_t> ServerPtr;

    static void
    setPort(uint16_t port)
    {
        port_ = port;
    }

    void
    SetUp()
    {
        server_.reset(new server_t(port_,
                                   256,
                                   xmlrpc::ServerType::MT));
    }

    void
    start_server()
    {
        server_->start();
    }


    void
    stop_server()
    {
        XmlRpc::XmlRpcClient client("localhost", port_);
        BOOST_SCOPE_EXIT((&client))
        {
            client.close();
        }
        BOOST_SCOPE_EXIT_END;
        {

            XmlRpc::XmlRpcValue params;
            XmlRpc::XmlRpcValue result;
            client.execute("system.stop",
                           params,
                           result);
            std::cout << (client.isFault() ? "Couldn't stop the server" : "Stopped the server") << std::endl;
        }
    }

    void
    join()
    {
        server_->join();
    }

    void
    TearDown()
    {}

    ServerPtr&
    server_ptr()
    {
        return server_;
    }


protected:
    static uint16_t port_;

    virtual void
    Run()
    {
        testing::Test::Run();
    }

    ServerPtr server_;
};

class WithRunningServer
{
public:
    WithRunningServer(XMLRPCTest& test)
        : test_(test)
    {
        test_.start_server();
    }
    WithRunningServer(const WithRunningServer&) = delete;

    WithRunningServer&
    operator=(const WithRunningServer&) = delete;

    ~WithRunningServer()
    {
        test_.stop_server();
        test_.join();
    }
    XMLRPCTest& test_;
};
#define WITH_RUNNING_SERVER WithRunningServer l__(*this)

}

#endif // XMLRPCTEST_H_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **

