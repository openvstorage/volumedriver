// Copyright 2015 iNuron NV
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

#include "FailOverCacheEnvironment.h"

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
