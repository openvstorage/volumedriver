#include "../src/Server.h"
#include <iostream>
#include <boost/program_options.hpp>
#include <youtils/Logger.h>
#include "SimpleTestServerMethods.h"

namespace po = boost::program_options;
using namespace xmlrpctest;
namespace x = XmlRpc;

void
nopstop()
{};


int main(int argc,
         char** argv)
{
    fungi::Logger::readLocalConfig("scrubberLogging",
                                   "INFO");

    po::options_description help("Help options");
    help.add_options()
        ("help,h", "produce help message");

    uint16_t port;
    bool mt;

    po::options_description desc("Required Options");
    desc.add_options()
        ("port", po::value<uint16_t>(&port)->default_value(2040), "port the xmlrpc server listens on")
        ("mt", po::value<bool>(&mt)->default_value(true),"run the xmlrpc server in multithreaded mode");

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


    xmlrpc::Server server(port,
                          256,
                          mt ? xmlrpc::ServerType::MT : xmlrpc::ServerType::ST);
    // Y42 Add a system stop method here!!!
    server.addMethod(new Counter());
    server.addMethod(new HangAbout(2));
    server.addMethod(new Fawlty());

    server.start();

    server.join();

}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **

