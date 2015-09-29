#include "Test.h"
#include "../src/XmlRpcClient.h"
#include <boost/scope_exit.hpp>
#include "../src/XmlRpcValue.h"
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
namespace xmlrpctest
{
namespace x = XmlRpc;

class SimpleTest : public Test
{};


TEST_F(SimpleTest, simpleCall)
{
    x::XmlRpcClient client("localhost", port_);

    BOOST_SCOPE_EXIT((&client))
    {
        client.close();
    }
    BOOST_SCOPE_EXIT_END;

    x::XmlRpcValue params;
    x::XmlRpcValue result;


    client.execute("counter",
                   params,
                   result);
    EXPECT_TRUE(not client.isFault());

}

struct HangingCall
{
    HangingCall(uint16_t port)
        :port_(port)
    {}

    void
    operator()()
    {
        x::XmlRpcClient client("localhost", port_);
        BOOST_SCOPE_EXIT((&client))
        {
            client.close();
        }
        BOOST_SCOPE_EXIT_END;

        x::XmlRpcValue params;
        x::XmlRpcValue result;

        client.execute("hangabout",
                       params,
                       result);

    }
    uint16_t port_;
};


TEST_F(SimpleTest, HangingCalls)
{
    std::vector<boost::thread*> threads;

    const unsigned num = 512;
    for(unsigned i = 0; i < num; ++i)
    {
        threads.push_back(new boost::thread(HangingCall(port_)));
    }
    BOOST_FOREACH(boost::thread* t, threads)
    {
        t->join();
        delete t;
    }
}

TEST_F(SimpleTest, FawltyCall)
{
   x::XmlRpcClient client("localhost", port_);

    BOOST_SCOPE_EXIT((&client))
    {
        client.close();
    }
    BOOST_SCOPE_EXIT_END;

    x::XmlRpcValue params;
    x::XmlRpcValue result;


    client.execute("fawlty",
                   params,
                   result);
    EXPECT_TRUE(client.isFault());
}

}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
