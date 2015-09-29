#include "Test.h"
#include "../src/XmlRpcClient.h"
#include <boost/scope_exit.hpp>
#include "../src/XmlRpcValue.h"
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <assert.h>
#include <youtils/Logging.h>
namespace xmlrpctest
{
namespace x = XmlRpc;

class TortureTest : public Test
{
public:
    DECLARE_LOGGER("TortureTest");

};

const char* methods [] =
{
    "counter",
    "hangabout",
    "fawlty"
};


struct SomeCall
{
    SomeCall(uint16_t port)
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

        const unsigned num_calls = sizeof(methods) / sizeof(const char*);

        unsigned val = floor(num_calls * drand48());
        assert(val <= num_calls - 1);

        client.execute("counter",
                       params,
                       result);
         if(val != 2)
         {
            ASSERT_FALSE(client.isFault());
         }

    }

    uint16_t port_;
};


TEST_F(TortureTest, test1)
{
    std::vector<boost::thread*> threads;

    const unsigned num = 256;
    if(numTortureTimes_ > 0)
    {

        for(unsigned i = 0; i < num; ++i)
        {
            usleep(1000);
            LOG_INFO("Starting thread: " << i);
            threads.push_back(new boost::thread(SomeCall(port_)));
        }
    }

    for(unsigned k = 0 ; k < numTortureTimes_; ++k)
    {
        for(unsigned i = 0; i < num; ++i)
        {
            threads[i]->join();
            delete threads[i];
            LOG_INFO("Refreshing thread: " << i);
            threads[i] = new boost::thread(SomeCall(port_));
        }
    }

    BOOST_FOREACH(boost::thread* t, threads)
    {
        t->join();
        delete t;
    }
}

}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
