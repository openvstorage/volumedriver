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

#include "ClientInterface.h"

#include "../FailOverCacheProxy.h"

namespace volumedriver
{

namespace failovercache
{

std::unique_ptr<ClientInterface>
ClientInterface::create(const FailOverCacheConfig& cfg,
                        const backend::Namespace& nspace,
                        const LBASize lba_size,
                        const ClusterMultiplier cmult,
                        const MaybeMilliSeconds& request_timeout,
                        const MaybeMilliSeconds& connect_timeout)
{
    return std::unique_ptr<ClientInterface>(new FailOverCacheProxy(cfg,
                                                                   nspace,
                                                                   lba_size,
                                                                   cmult,
                                                                   request_timeout,
                                                                   connect_timeout));
}

}

}
