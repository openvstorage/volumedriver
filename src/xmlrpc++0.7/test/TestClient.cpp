#include <string>
#include <iostream>
#include <sstream>
#include <cassert>
#include <youtils/Logger.h>
#include <boost/program_options.hpp>
#include "Test.h"
#include "../src/XmlRpcClient.h"
#include "../src/XmlRpcValue.h"
#include <boost/scope_exit.hpp>
namespace po = boost::program_options;
using namespace xmlrpctest;

int main(int argc, char **argv)
{

    fungi::Logger::readLocalConfig("xmlrpcLogging",
                                   "DEBUG");

    if(geteuid() == 0)
    {
        LOG_FFATAL("vd_test","These tests cannot be run as root");
        exit(1);
    }


    testing::InitGoogleTest(&argc, argv);

    po::options_description help("Help options");
    help.add_options()
        ("help,h", "produce help message");


    po::options_description desc("Required Options");

    uint16_t port;
    bool stop;
    uint64_t torture;

    desc.add_options()
        ("port", po::value<uint16_t>(&port)->default_value(2040), "port the xmlrpc server listens on")
        ("torture", po::value<uint64_t>(&torture)->default_value(0), "number of times to run the torture test")
        ("stop", po::value<bool>(&stop)->default_value(false),"whether to stop the xmlrpc server after the tests");
    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc,argv, help), vm);
        po::notify(vm);
        if(vm.count("help"))
        {
            std::cout << desc << std::endl;
            return 1;
        }
    }
    catch(...)
    {
        // Not a help option.
    }

    po::store(po::parse_command_line(argc,argv, desc), vm);
    po::notify(vm);

    Test::setPort(port);
    Test::setTortureTimes(torture);

    int result = RUN_ALL_TESTS();

    if(stop)
    {
        XmlRpc::XmlRpcClient client("localhost", port);
        BOOST_SCOPE_EXIT((&client))
        {
            client.close();
        }
        BOOST_SCOPE_EXIT_END;

        XmlRpc::XmlRpcValue params;
        XmlRpc::XmlRpcValue result;
        client.execute("system.stop",
                       params,
                       result);
        std::cout << (client.isFault() ? "Couldn't stop the server" : "Stopped the server") << std::endl;
    }


    exit(result);

}
// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **

