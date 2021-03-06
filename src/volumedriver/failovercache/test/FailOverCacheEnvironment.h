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
