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

#ifndef GLOBAL_LOCK_SERVICE_H
#define GLOBAL_LOCK_SERVICE_H

#include "GlobalLockedCallable.h"
#include "IOException.h"
#include "TimeDurationType.h"

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace youtils
{

class GlobalLockService
{
public:
    MAKE_EXCEPTION(GlobalLockServiceException, fungi::IOException);
    MAKE_EXCEPTION(InsufficientConnectionDetails, GlobalLockServiceException);
    MAKE_EXCEPTION(NoSuchLock, GlobalLockServiceException);

    // Nobody gets the lock before your lost_lock_callback has been called and it has returned...
    typedef void (*lost_lock_callback)(void* data,
                                       const std::string& reason);

    GlobalLockService(const GracePeriod& grace_period,
                      lost_lock_callback callback = 0,
                      void* data = 0)
        : callback_(callback),
          data_(data),
          grace_period_(grace_period)
    {}

    virtual ~GlobalLockService() = 0;

    virtual bool
    lock() = 0;

    virtual void
    unlock() = 0;

protected:
    lost_lock_callback callback_;
    void* data_;
    boost::posix_time::time_duration grace_period_;
};

}

#endif // GLOBAL_LOCK_SERVICE_H

// Local Variables: **
// mode: c++ **
// End: **
