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

#include "../DtlServer.h"
#include "../../DtlTransport.h"

#include <gtest/gtest.h>
#include <string>
#include <memory>

namespace boost
{
class thread;
}

namespace distributed_transaction_log_test
{

class DtlEnvironment
    : testing::Environment
{
public:
    DtlEnvironment(const boost::optional<std::string>& host,
                             const uint16_t port,
                             const volumedriver::DtlTransport);

    ~DtlEnvironment();

    virtual void
    SetUp();

    virtual void
    TearDown();

private:
    const boost::optional<std::string> host_;
    const uint16_t port_;
    const volumedriver::DtlTransport transport_;
    const boost::filesystem::path path_;
    std::unique_ptr<DtlServer> server_;
    std::unique_ptr<boost::thread> thread_;
};

}

#endif // FAILOVERCACHE_ENVIRONMENT_H_
