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

#ifndef DTL_TEST_SETUP_H_
#define DTL_TEST_SETUP_H_

#include <memory>
#include <set>

#include <boost/filesystem.hpp>

#include <youtils/Logging.h>

#include "../DtlConfig.h"
#include "../DtlProxy.h"
#include "../DtlTransport.h"
#include "../distributed-transaction-log/Backend.h"
#include "../distributed-transaction-log/Acceptor.h"
#include "../distributed-transaction-log/DtlProtocol.h"

class VolumeDriverTest;

namespace volumedrivertest
{

class DtlTestSetup;

class FailOverCacheTestContext
{
private:
    friend class DtlTestSetup;

    FailOverCacheTestContext(DtlTestSetup& setup,
                             const boost::optional<std::string>& addr,
                             const uint16_t port);

    FailOverCacheTestContext(const FailOverCacheTestContext&) = delete;

    FailOverCacheTestContext&
    operator=(const FailOverCacheTestContext&) = delete;

    DtlTestSetup& setup_;
    const boost::optional<std::string> addr_;
    const uint16_t port_;
    distributed_transaction_log::Acceptor acceptor_;
    std::unique_ptr<fungi::SocketServer> server_;

public:
    ~FailOverCacheTestContext();

    uint16_t
    port() const
    {
        return port_;
    }

    volumedriver::DtlConfig
    config(const volumedriver::DtlMode) const;

    std::shared_ptr<distributed_transaction_log::Backend>
    backend(const backend::Namespace&);
};

typedef std::shared_ptr<FailOverCacheTestContext> foctest_context_ptr;

class DtlTestSetup
{
    friend class FailOverCacheTestContext;
    friend class ::VolumeDriverTest;

public:
    explicit DtlTestSetup(const boost::optional<boost::filesystem::path>&);

    ~DtlTestSetup();

    DtlTestSetup(const DtlTestSetup&) = delete;

    DtlTestSetup&
    operator=(const DtlTestSetup&) = delete;

    foctest_context_ptr
    start_one_foc();

    static uint16_t
    port_base()
    {
        return port_base_;
    }

    uint16_t
    get_next_foc_port() const;

    static std::string
    host()
    {
        return addr_;
    }

    static volumedriver::DtlTransport
    transport()
    {
        return transport_;
    }

    const boost::optional<boost::filesystem::path> path;

private:
    DECLARE_LOGGER("DtlTestSetup");

    static std::string addr_;
    static uint16_t port_base_;
    static volumedriver::DtlTransport transport_;

    typedef std::set<uint16_t> set_type;
    set_type ports_;

    void
    release_port_(uint16_t port)
    {
        ports_.erase(port);
    }
};

}

#endif //!DTL_TEST_SETUP_H_

// Local Variables: **
// mode: c++ **
// End: **
