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

#ifndef FAILOVERCACHE_ENVIRONMENT_H_
#define FAILOVERCACHE_ENVIRONMENT_H_

#include "../FailOverCacheServer.h"
#include "../../FailOverCacheTransport.h"

#include <gtest/gtest.h>
#include <string>
#include <memory>

namespace boost
{
class thread;
}

namespace failovercachetest
{

class FailOverCacheEnvironment
    : testing::Environment
{
public:
    FailOverCacheEnvironment(const boost::optional<std::string>& host,
                             const uint16_t port,
                             const volumedriver::FailOverCacheTransport);

    ~FailOverCacheEnvironment();

    virtual void
    SetUp();

    virtual void
    TearDown();

private:
    const boost::optional<std::string> host_;
    const uint16_t port_;
    const volumedriver::FailOverCacheTransport transport_;
    const boost::filesystem::path path_;
    std::unique_ptr<FailOverCacheServer> server_;
    std::unique_ptr<boost::thread> thread_;
};

}

#endif // FAILOVERCACHE_ENVIRONMENT_H_
