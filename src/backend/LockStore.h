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

#ifndef BACKEND_LOCK_STORE_H_
#define BACKEND_LOCK_STORE_H_

#include "BackendInterface.h"

#include <youtils/GlobalLockStore.h>
#include <youtils/HeartBeatLock.h>
#include <youtils/Logging.h>

namespace backend
{

class LockStore
    : public youtils::GlobalLockStore
{
public:
    explicit LockStore(BackendInterfacePtr);

    ~LockStore() = default;

    LockStore(const LockStore&) = delete;

    LockStore&
    operator=(const LockStore&) = delete;

    bool
    exists() override final;

    std::tuple<youtils::HeartBeatLock, std::unique_ptr<youtils::UniqueObjectTag>>
    read() override final;

    std::unique_ptr<youtils::UniqueObjectTag>
    write(const youtils::HeartBeatLock&,
          const youtils::UniqueObjectTag*) override final;

    const std::string&
    name() const override final;

    void
    erase() override final;

private:
    DECLARE_LOGGER("BackendLockStore");

    BackendInterfacePtr bi_;
};

}

#endif // !BACKEND_LOCK_STORE_H_
