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

#include "FailOverCacheServer.h"

#include <boost/optional/optional_io.hpp>

namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace vd = volumedriver;

FailOverCacheServer::FailOverCacheServer(int argc,
                                         char** argv)
    : FailOverCacheServer(fromArgcArgv(argc, argv))
{}

FailOverCacheServer::FailOverCacheServer(const constructor_type& c)
    : MainHelper(c)
    , desc_("Required Options")
    , transport_(vd::FailOverCacheTransport::TCP)
    , running_(false)
{
    logger_ = &MainHelper::getLogger__();

    desc_.add_options()
        ("path",
         po::value<fs::path>(&path_)->required(),
         "path of the failovercache server")
        ("address",
         po::value<std::string>(&addr_),
         "address to bind the server to")
        ("port",
         po::value<uint16_t>(&port_)->default_value(23096),
         "port of the failovercache server")
        ("transport",
         po::value<vd::FailOverCacheTransport>(&transport_)->default_value(transport_),
         "transport type of the failovercache server (TCP|RSocket)")
        ("daemonize,D",
         "run as a daemon");
}

void
FailOverCacheServer::log_extra_help(std::ostream& strm)
{
    strm << desc_;
}

bool
FailOverCacheServer::running() const
{
    return running_;
}

void
FailOverCacheServer::parse_command_line_arguments()
{
    parse_unparsed_options(desc_,
                           youtils::AllowUnregisteredOptions::F,
                           vm_);
}

void
FailOverCacheServer::setup_logging()
{
    MainHelper::setup_logging();
}

int
FailOverCacheServer::run()
{
    if(not fs::exists(path_))
    {
        throw fungi::IOException("failovercache directory does not exist",
                                 path_.string().c_str());
    }

    if(not fs::is_directory(path_))
    {
        throw fungi::IOException("failovercache path is not a directory",
                                 path_.string().c_str());
    }

    if (vm_.count("daemonize"))
    {
        const int ret = ::daemon(0, 0);
        if (ret)
        {
            throw fungi::IOException("failed to daemonize", strerror(errno));
        }
    }

    boost::optional<std::string> addr;
    if (not addr_.empty())
    {
        addr = addr_;
    }

    LOG_INFO( "starting, server path: " << path_ <<
              ", address to bind to: " << addr <<
              ", port: " << port_ <<
              ", transport type: " << transport_);

    acceptor.reset(new failovercache::FailOverCacheAcceptor(path_));

    LOG_INFO("Running the SocketServer");

    s = fungi::SocketServer::createSocketServer(*acceptor,
                                                addr,
                                                port_,
                                                transport_ == vd::FailOverCacheTransport::RSocket);
    running_ = true;

    LOG_INFO("Waiting for the SocketServer");
    s->join();

    LOG_INFO("Exiting SocketServer");
    s.reset();

    return 0;
}

// Good grief! This thing used to be called in a signal handler(!), but now it's only used
// by tests. And even that still feels wrong as the code has a number of issues.
//
// If you want to (have to?) fix it, start with the fungi::SocketServer shutdown code
// (why in $DEITY's name does it have to use SIGUSR1 internally!?), then get rid of
// the sleep(3) and promise not to call this in signal handler context
// (signalfd might help there).
void
FailOverCacheServer::stop_()
{
    if(s)
    {
        s->stop();
        // fungi::SocketServer::stop() is protected against multiple invocations
        // s = 0;
    }
    else
    {
        LOG_INFO("No socket server");
    }

    if(acceptor)
    {
        const fs::path cleanup_path(acceptor->root_);
        acceptor.reset();
        sleep(3);

        fs::directory_iterator end;
        for(fs::directory_iterator it(cleanup_path); it != end; ++it)
        {
            LOG_INFO("Cleaning up " << it->path());
            fs::remove_all(it->path());
        }
    }
    else
    {
        LOG_INFO("No acceptor, stopping without cleanup");
    }
}

youtils::Logger::logger_type*
FailOverCacheServer::logger_;
