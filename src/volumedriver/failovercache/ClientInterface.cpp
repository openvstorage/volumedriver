// Copyright (C) 2017 iNuron NV
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

#include "CapnProtoClient.h"
#include "ClientInterface.h"

#include "../FailOverCacheProxy.h"
#include "../OwnerTag.h"
#include "../VolManager.h"

#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>

#include <youtils/Catchers.h>

namespace volumedriver
{

namespace failovercache
{

std::unique_ptr<ClientInterface>
ClientInterface::create(boost::asio::io_service& io_service,
                        const youtils::ImplicitStrand implicit_strand,
                        const FailOverCacheConfig& cfg,
                        const backend::Namespace& nspace,
                        const OwnerTag owner_tag,
                        const LBASize lba_size,
                        const ClusterMultiplier cmult,
                        const MaybeMilliSeconds& request_timeout,
                        const MaybeMilliSeconds& connect_timeout)
{
    try
    {
        return std::unique_ptr<ClientInterface>(new CapnProtoClient(cfg,
                                                                    implicit_strand,
                                                                    owner_tag,
                                                                    io_service,
                                                                    nspace,
                                                                    lba_size,
                                                                    cmult,
                                                                    request_timeout,
                                                                    connect_timeout));
    }
    catch (boost::system::system_error& e)
    {
        if (e.code() == boost::asio::error::connection_reset)
        {
            LOG_WARN(nspace << ": server reset connection - outdated server? " << e.what());
        }
        else
        {
            LOG_ERROR(nspace << ": failed to instantiate CapnProtoClient: " << e.what());
            throw;
        }
    }
    CATCH_STD_ALL_LOG_RETHROW(nspace << ": failed to instantiate CapnProtoClient");

    LOG_INFO(nspace << ": falling back to old client");
    return std::unique_ptr<ClientInterface>(new FailOverCacheProxy(cfg,
                                                                   nspace,
                                                                   lba_size,
                                                                   cmult,
                                                                   request_timeout,
                                                                   connect_timeout));
}

}

}
