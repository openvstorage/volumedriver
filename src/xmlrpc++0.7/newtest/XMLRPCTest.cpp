#include "XMLRPCTest.h"
namespace xmlrpctest
{
namespace x = XmlRpc;


uint16_t
XMLRPCTest::port_ = 0;

class ThrowingCall : public x::XmlRpcServerMethod
{
public:
    explicit ThrowingCall()
        : x::XmlRpcServerMethod("nothing")
    {}

    ThrowingCall(const ThrowingCall&) = delete;
    ThrowingCall&
    operator=(const ThrowingCall&) = delete;

private:
    virtual void
    execute(x::XmlRpcValue&  /* params */,
            x::XmlRpcValue& /* result */)
    {
        throw x::XmlRpcException("Throwing call, what did you expect");
    }
};

class HangingCall : public x::XmlRpcServerMethod
{
public:
    explicit HangingCall()
        : x::XmlRpcServerMethod("hangingCall")
        , b_(false)
        , inside_(false)
    {}

    HangingCall(const HangingCall&) = delete;
    HangingCall& operator=(const HangingCall&) = delete;

    void
    unlock()
    {
        {
            boost::unique_lock<boost::mutex> lock(m_);
            b_ = true;
        }
        c_.notify_one();
    }

    virtual std::string
    help()
    {
        return "do nothing for a while";
    }

    bool
    inside() const
    {
        return inside_;
    }

private:
    virtual void
    execute(x::XmlRpcValue&  /* params */,
            x::XmlRpcValue& /* result */)
    {
        boost::unique_lock<boost::mutex> lock(m_);
        inside_ = true;
        while(not b_)
        {
            c_.wait(lock);
        }
        inside_ = false;

    }

    boost::mutex m_;
    boost::condition_variable c_;
    bool b_;
    bool inside_;
};

struct NoOpSystemStop
{
    void
    stop()
    {}
};

TEST_F(XMLRPCTest, ServerNotRunning)
{
    server_ptr()->addMethod(new ThrowingCall());
    NoOpSystemStop stop;

    server_ptr()->addMethod(new xmlrpc::SystemStop<NoOpSystemStop,
                                                   &NoOpSystemStop::stop>(*(server_ptr()),
                                                                          stop));

    EXPECT_EQ(server_ptr()->getServer()->num_connections(), 0);
    {
        ThreadedXMLRCPCall throwing("nothing",
                                    port_);

        throwing.fault(false);
        throwing.start(5);
        throwing.join();

        EXPECT_TRUE(throwing.fault());
        EXPECT_FALSE(throwing.clientFault());

    }

    {
        WITH_RUNNING_SERVER;

        {
            ThreadedXMLRCPCall throwing("nothing",
                                        port_);

            throwing.fault(false);
            throwing.start();
            throwing.join();

            EXPECT_FALSE(throwing.fault());
            EXPECT_TRUE(throwing.clientFault());
        }
    }

    EXPECT_EQ(server_ptr()->getServer()->num_connections(), 0);
}

TEST_F(XMLRPCTest, Nothing)
{
    // Not really good, why not pass in a unique_ptr?
    HangingCall* hanging_call = new HangingCall();
    server_ptr()->addMethod(hanging_call);
    NoOpSystemStop stop;

    server_ptr()->addMethod(new xmlrpc::SystemStop<NoOpSystemStop,
                                                   &NoOpSystemStop::stop>(*(server_ptr()),
                                                                          stop));

    WITH_RUNNING_SERVER;

    EXPECT_EQ(server_ptr()->getServer()->num_connections(), 0);

    ThreadedXMLRCPCall hang1("hangingCall",
                             port_);
    hang1.start();
    while(not hanging_call->inside())
    {
        sleep(1);
    }

    EXPECT_EQ(server_ptr()->getServer()->num_connections(), 1);

    hanging_call->unlock();
    hang1.join();

    EXPECT_EQ(server_ptr()->getServer()->num_connections(), 0);

}

struct PluggedSystemStop
{
    void
    stop()
    {
        boost::unique_lock<boost::mutex> lock(m_);
        inside_ = true;
        while(not b_)
        {
            c_.wait(lock);
        }
        inside_ = false;
    }

    void
    unplug()
    {
        {
            boost::unique_lock<boost::mutex> lock(m_);
            b_ = true;
        }
        c_.notify_one();
    }

    bool
    inside() const
    {
        return inside_;
    }

    boost::mutex m_;
    boost::condition_variable c_;
    bool b_;
    bool inside_;
};

TEST_F(XMLRPCTest, ManyStops)
{
    //    server_ptr()->addMethod(hanging_call);
    PluggedSystemStop stop;
    server_ptr()->addMethod(new xmlrpc::SystemStop<PluggedSystemStop,
                                                   &PluggedSystemStop::stop>(*(server_ptr()),
                                                                          stop));
    WITH_RUNNING_SERVER;
    boost::ptr_vector<ThreadedXMLRCPCall> calls;
    typedef boost::ptr_vector<ThreadedXMLRCPCall>::iterator ptr_vec_it;
    const unsigned num_first = 10;
    ASSERT_TRUE(num_first >= 1);

    for(unsigned i = 0; i < num_first; ++i)
    {
        calls.push_back(new ThreadedXMLRCPCall("system.stop",
                                                  port_));
    }

    for(ptr_vec_it it = calls.begin(); it != calls.end(); ++it)
    {
        it->start();
    }

    while(not stop.inside())
    {
        sleep(1);
    }

    unsigned  running = 0;
    unsigned not_running = 0;

    for(ptr_vec_it it = calls.begin(); it != calls.end(); ++it)
    {
        if(it->timed_join(boost::posix_time::seconds(1)))
        {
            ++not_running;
            ASSERT(it->fault() or
                   it->clientFault());
        }
        else
        {
            ++running;
        }
    }

    ASSERT_EQ(running,1);
    ASSERT_EQ(not_running, num_first - 1);

    const unsigned num_second = 10;

    boost::ptr_vector<ThreadedXMLRCPCall> calls2;
    for(unsigned i = 0; i < num_second; ++i)
    {
        calls2.push_back(new ThreadedXMLRCPCall("system.stop",
                                                  port_));
    }


    for(ptr_vec_it it = calls2.begin(); it != calls2.end(); ++it)
    {
        it->start();
    }

    for(ptr_vec_it it = calls2.begin(); it != calls2.end(); ++it)
    {
        it->join();
        ASSERT_TRUE(it->fault() or
                    it->clientFault());
    }
    stop.unplug();


}

struct ThrowingStop
{
    ThrowingStop()
        : throwing_(true)
    {}

    void
    stop()
    {
        if(throwing_)
        {
            throw fungi::IOException("Keep going, I'm almost there.");
        }
    }

    void
    unplug()
    {
        throwing_ = false;
    }

    bool throwing_;

};

TEST_F(XMLRPCTest, ThrowingStop)
{
    //    server_ptr()->addMethod(hanging_call);
    ThrowingStop stop;
    server_ptr()->addMethod(new xmlrpc::SystemStop<ThrowingStop,
                                                   &ThrowingStop::stop>(*(server_ptr()),
                                                                       stop));
    WITH_RUNNING_SERVER;
    const unsigned num_tests = 5;

    for(unsigned i = 0; i < num_tests; ++i)
    {
        ThreadedXMLRCPCall t("system.stop",
                             port_);
        t.start();
        t.join();
        std::cout << t.result().toXml() << std::endl;
    }
    stop.unplug();
    {

        ThreadedXMLRCPCall t("system.stop",
                             port_);
        t.start();
        t.join();
        std::cout << t.result().toXml() << std::endl;
    }

    {

        ThreadedXMLRCPCall t("system.stop",
                             port_);
        t.start();
        t.join();
        std::cout << t.result().toXml() << std::endl;
    }
}

struct SlowStop
{
    SlowStop()

    {}

    void
    stop()
    {
        sleep(5);
    }

};

TEST_F(XMLRPCTest, SlowStop)
{
    //    server_ptr()->addMethod(hanging_call);
    SlowStop stop;
    server_ptr()->addMethod(new xmlrpc::SystemStop<SlowStop,
                                                   &SlowStop::stop>(*(server_ptr()),
                                                                       stop));
    WITH_RUNNING_SERVER;
    const unsigned num_tests = 5;
    boost::ptr_vector<ThreadedXMLRCPCall> calls;
    typedef boost::ptr_vector<ThreadedXMLRCPCall>::iterator ptr_vec_it;

    for(unsigned i = 0; i < num_tests; ++i)
    {
        calls.push_back(new ThreadedXMLRCPCall("system.stop",
                                               port_));
        calls.back().start();

    }

    for(unsigned i = 0; i < num_tests; ++i)
    {
        calls.push_back(new ThreadedXMLRCPCall("system.stop",
                                               port_));
        calls.back().start();

    }

    for(ptr_vec_it it = calls.begin(); it != calls.end(); ++it)
    {
        it->join();
        std::cout << it->result().toXml() << std::endl;
    }
}


}
// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
