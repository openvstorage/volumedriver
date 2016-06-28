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

#include "DtlClientInterface.h"
#include "DtlAsyncBridge.h"
#include "DtlSyncBridge.h"

#include <youtils/Assert.h>

namespace volumedriver
{

using Ptr = std::unique_ptr<DtlClientInterface>;

Ptr
DtlClientInterface::create(const DtlMode mode,
                           const LBASize lba_size,
                           const ClusterMultiplier cluster_multiplier,
                           const size_t max_entries,
                           const std::atomic<unsigned>& write_trigger)
{
    switch (mode)
    {
    case DtlMode::Asynchronous:
        return Ptr(new DtlAsyncBridge(lba_size,
                                                cluster_multiplier,
                                                max_entries,
                                                write_trigger));
    case DtlMode::Synchronous:
        return Ptr(new DtlSyncBridge(max_entries));
    }

    VERIFY(0 == "Someone added a new DtlMode and forgot to update this function?");
}

} // namespace volumedriver
