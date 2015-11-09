// Copyright 2015 iNuron NV
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

    virtual std::tuple<HeartBeatLock, GlobalLockTag>
    read() override final;

    virtual GlobalLockTag
    write(const HeartBeatLock&,
          const boost::optional<GlobalLockTag>&) override final;

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
