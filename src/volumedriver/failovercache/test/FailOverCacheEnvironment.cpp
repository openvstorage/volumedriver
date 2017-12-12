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

#include "FailOverCacheEnvironment.h"

#include "../FailOverCacheAcceptor.h"
#include "../FailOverCacheServer.h"

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

#include <youtils/FileUtils.h>

namespace failovercachetest
{

namespace vd = volumedriver;

FailOverCacheEnvironment::FailOverCacheEnvironment(const boost::optional<std::string>& host,
                                                   const uint16_t port,
                                                   const vd::FailOverCacheTransport transport)
    : host_(host)
    , port_(port)
    , transport_(transport)
    , path_(youtils::FileUtils::temp_path() / (std::string("failovercacheserver-test-") +
                                               boost::lexical_cast<std::string>(getpid())))
{
    youtils::FileUtils::checkDirectoryEmptyOrNonExistant(path_);
}

FailOverCacheEnvironment::~FailOverCacheEnvironment()
{
    youtils::FileUtils::removeAllNoThrow(path_);
}

void
FailOverCacheEnvironment::SetUp()
{
    std::vector<std::string> args;

    if (host_)
    {
        args.push_back("--address");
        args.push_back(*host_);
    }

    args.push_back("--port");
    args.push_back(boost::lexical_cast<std::string>(port_));

    args.push_back("--path");
    args.push_back(path_.string());

    args.push_back("--transport");
    args.push_back(boost::lexical_cast<std::string>(transport_));

    const std::string executable_name("failovercacher_server_tester");

    server_.reset(new FailOverCacheServer(std::make_pair(executable_name, args)));

    thread_.reset(new boost::thread(boost::ref(*server_)));
    while(not server_->running())
    {
        sleep(1);
    }
}

void
FailOverCacheEnvironment::TearDown()
{
    server_->stop_();
    thread_->join();
    server_.reset();
    thread_.reset();
}

}
