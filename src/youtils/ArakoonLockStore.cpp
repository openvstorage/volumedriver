// Copyright 2015 Open vStorage NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ArakoonLockStore.h"
#include "Weed.h"

#include <boost/lexical_cast.hpp>

namespace youtils
{

namespace ara = arakoon;

using namespace std::literals::string_literals;

namespace
{

GlobalLockTag
global_lock_tag(const std::string& s)
{
    return boost::lexical_cast<GlobalLockTag>(Weed(reinterpret_cast<const uint8_t*>(s.data()),
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

std::tuple<HeartBeatLock, GlobalLockTag>
ArakoonLockStore::read()
{
    const std::string s(larakoon_->get<std::string,
                                       std::string>(make_key_()));

    return std::make_tuple(HeartBeatLock(s),
                           global_lock_tag(s));
}

GlobalLockTag
ArakoonLockStore::write(const HeartBeatLock& lock,
                        const boost::optional<GlobalLockTag>& lock_tag)
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

                     if (global_lock_tag(old_val) != *lock_tag)
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
