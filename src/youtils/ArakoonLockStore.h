// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
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
