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

#include "ArakoonLockStore.h"
#include "ObjectMd5.h"
#include "Weed.h"

namespace youtils
{

namespace ara = arakoon;

using namespace std::literals::string_literals;

namespace
{

std::unique_ptr<UniqueObjectTag>
global_lock_tag(const std::string& s)
{
    return std::make_unique<ObjectMd5>(Weed(reinterpret_cast<const uint8_t*>(s.data()),
                                            s.size()));
}

}

ArakoonLockStore::ArakoonLockStore(std::shared_ptr<LockedArakoon> larakoon,
                                   const std::string& nspace)
    : GlobalLockStore()
    , larakoon_(larakoon)
    , nspace_(nspace)
{}

std::string
ArakoonLockStore::make_key_() const
{
    return "GlobalLocks/"s + nspace_;
}

bool
ArakoonLockStore::exists()
{
    return larakoon_->exists(make_key_());
}

void
ArakoonLockStore::erase()
{
    return larakoon_->delete_prefix(make_key_());
}

const std::string&
ArakoonLockStore::name() const
{
    return nspace_;
}

std::tuple<HeartBeatLock, std::unique_ptr<UniqueObjectTag>>
ArakoonLockStore::read()
{
    const std::string s(larakoon_->get<std::string,
                                       std::string>(make_key_()));

    return std::make_tuple(HeartBeatLock(s),
                           global_lock_tag(s));
}

std::unique_ptr<UniqueObjectTag>
ArakoonLockStore::write(const HeartBeatLock& lock,
                        const UniqueObjectTag* lock_tag)
{
    std::string new_val;
    lock.save(new_val);

    const std::string key(make_key_());

    auto fun([&](arakoon::sequence& seq)
             {
                 if (lock_tag)
                 {
                     const std::string
                         old_val(larakoon_->get<std::string,
                                                std::string>(key));

                     if (*global_lock_tag(old_val) != *lock_tag)
                     {
                         LOG_INFO(nspace_ << ": lock has changed");
                         throw GlobalLockStore::LockHasChanged("lock has changed",
                                                               nspace_.c_str());
                     }

                     seq.add_assert(key,
                                    old_val);
                 }
                 else
                 {
                     seq.add_assert(key,
                                    ara::None());
                 }

                 seq.add_set(key,
                             new_val);
             });

    try
    {
        larakoon_->run_sequence("writing global lock",
                                std::move(fun));
    }
    catch (ara::error_assertion_failed&)
    {
        LOG_INFO(nspace_ << ": lock has changed");
        throw GlobalLockStore::LockHasChanged("lock has changed",
                                              nspace_.c_str());
    }

    return global_lock_tag(new_val);
}

}
