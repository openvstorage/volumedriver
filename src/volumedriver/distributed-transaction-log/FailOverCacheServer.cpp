// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

#include "FailOverCacheServer.h"

#include <boost/optional/optional_io.hpp>

namespace dtl = distributed_transaction_log;
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
         po::value<fs::path>(&path_),
         "directory for the disk cache, omitting it selects memory caching")
        ("address",
         po::value<std::string>(&addr_),
         "address to bind the DTL to")
        ("port",
         po::value<uint16_t>(&port_)->default_value(23096),
         "port to use for the DTL")
        ("transport",
         po::value<vd::FailOverCacheTransport>(&transport_)->default_value(transport_),
         "transport type for the DTL (TCP|RSocket)")
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
    MainHelper::setup_logging("dtl_server");
}

int
FailOverCacheServer::run()
{
    boost::optional<fs::path> path;
    if (not path_.empty())
    {
        path = path_;
    }

    if (path)
    {
        if(not fs::exists(*path))
        {
            throw fungi::IOException("failovercache directory does not exist",
                                     path->string().c_str());
        }

        if(not fs::is_directory(*path))
        {
            throw fungi::IOException("failovercache path is not a directory",
                                     path->string().c_str());
        }
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

    LOG_INFO( "starting, server path: " << path <<
              ", address to bind to: " << addr <<
              ", port: " << port_ <<
              ", transport type: " << transport_);

    acceptor = std::make_unique<dtl::Acceptor>(path);

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

    acceptor = nullptr;
}

youtils::Logger::logger_type*
FailOverCacheServer::logger_;
