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

#ifndef BACKEND_LOCK_STORE_H_
#define BACKEND_LOCK_STORE_H_

#include "BackendInterface.h"
#include "GlobalLockStore.h"
#include "Lock.h"
#include "LockTag.h"

#include <youtils/Logging.h>

namespace backend
{

class LockStore
    : public GlobalLockStore
{
public:
    explicit LockStore(BackendInterfacePtr);

    ~LockStore() = default;

    LockStore(const LockStore&) = delete;

    LockStore&
    operator=(const LockStore&) = delete;

    bool
    exists() override final;

    std::tuple<Lock, LockTag>
    read() override final;

    LockTag
    write(const Lock&,
          const boost::optional<LockTag>&) override final;

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
