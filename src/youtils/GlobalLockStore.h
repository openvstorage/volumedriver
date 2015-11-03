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
