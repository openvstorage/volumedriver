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

#ifndef DENY_LOCK_SERVICE_H_
#define DENY_LOCK_SERVICE_H_

#include "../GlobalLockService.h"
#include "../WithGlobalLock.h"

namespace youtilstest
{
class DenyLockService : public youtils::GlobalLockService
{
public:
    DenyLockService(const youtils::GracePeriod& grace_period,
                    lost_lock_callback callback,
                     void* data)
        :GlobalLockService(grace_period,
                           callback,
                           data)
    {}


    template<youtils::ExceptionPolicy policy,
             typename Callable,
             std::string(Callable::*info_member_function)() = &Callable::info()>
    using WithGlobalLock = youtils::WithGlobalLock<policy,
                                                   Callable,
                                                   info_member_function,
                                                   DenyLockService>;

    virtual bool
    lock()
    {
        return false;
    }

    virtual void
    unlock()
    {};
};

}

#endif // DENY_LOCK_SERVICE_H_

// Local Variables: **
// mode: c++ **
// End: **
