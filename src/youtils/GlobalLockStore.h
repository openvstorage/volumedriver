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

#ifndef BACKEND_GLOBAL_LOCK_STORE_H_
#define BACKEND_GLOBAL_LOCK_STORE_H_

#include "HeartBeatLock.h"
#include "UniqueObjectTag.h"

#include <boost/shared_ptr.hpp>

#include <memory>

namespace youtils
{

struct GlobalLockStore
{
    MAKE_EXCEPTION(LockHasChanged,
                   fungi::IOException);

    virtual ~GlobalLockStore() = default;

    virtual bool
    exists() = 0;

    virtual std::tuple<HeartBeatLock, std::unique_ptr<UniqueObjectTag>>
    read() = 0;

    virtual std::unique_ptr<UniqueObjectTag>
    write(const HeartBeatLock&,
          const UniqueObjectTag*) = 0;

    virtual const std::string&
    name() const = 0;

    virtual void
    erase() = 0;
};

using GlobalLockStorePtr = boost::shared_ptr<GlobalLockStore>;

}

#endif // !BACKEND_GLOBAL_LOCK_STORE_H_

// Local Variables: **
// mode: c++ **
// End: **
