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

#ifndef BACKEND_GLOBAL_LOCK_STORE_H_
#define BACKEND_GLOBAL_LOCK_STORE_H_

#include "HeartBeatLock.h"
#include "GlobalLockTag.h"

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

    virtual std::tuple<HeartBeatLock, GlobalLockTag>
    read() = 0;

    virtual GlobalLockTag
    write(const HeartBeatLock&,
          const boost::optional<GlobalLockTag>&) = 0;

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
