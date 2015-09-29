#ifndef XML_RPC_TEST_H
#define XML_RPC_TEST_H

#include <gtest/gtest.h>
#include <youtils/IOException.h>
#include <stdexcept>
#include <boost/filesystem.hpp>

namespace xmlrpctest
{
    class TestConfig
    {};

    class Test : public testing::TestWithParam<TestConfig>
    {
    public:
        static void
        setPort(uint16_t port)
        {
            port_ = port;
        }
        static void
        setTortureTimes(uint64_t numTortureTimes)
        {
            numTortureTimes_ = numTortureTimes;
        }

    protected:
        static uint16_t port_;

        static uint64_t numTortureTimes_;

    private:
        virtual void
        Run()
        {
            testing::Test::Run();
        }
    };
}


#endif // XML_RPC_TEST_H



// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
