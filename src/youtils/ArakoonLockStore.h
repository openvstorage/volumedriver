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

#ifndef YT_ARAKOON_LOCK_STORE_H_
#define YT_ARAKOON_LOCK_STORE_H_

#include "GlobalLockStore.h"
#include "IOException.h"
#include "LockedArakoon.h"
#include "Logging.h"

namespace youtils
{

class ArakoonLockStore
    : public GlobalLockStore
{
public:
    ArakoonLockStore(std::shared_ptr<LockedArakoon> larakoon,
                     const std::string& nspace);

    ~ArakoonLockStore() = default;

    ArakoonLockStore(const ArakoonLockStore&) = delete;

    ArakoonLockStore&
    operator=(const ArakoonLockStore&) = delete;

    virtual bool
    exists() override final;

    virtual std::tuple<HeartBeatLock, std::unique_ptr<UniqueObjectTag>>
    read() override final;

    virtual std::unique_ptr<UniqueObjectTag>
    write(const HeartBeatLock&,
          const UniqueObjectTag*) override final;

    virtual const std::string&
    name() const override final;

    virtual void
    erase() override final;

private:
    DECLARE_LOGGER("ArakoonLockStore");

    std::shared_ptr<LockedArakoon> larakoon_;
    const std::string nspace_;

    std::string
    make_key_() const;
};

}

#endif // !YT_ARAKOON_LOCK_STORE_H_
