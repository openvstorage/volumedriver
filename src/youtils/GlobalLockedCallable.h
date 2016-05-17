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

#ifndef GLOBAL_LOCKED_CALLABLE_H_
#define GLOBAL_LOCKED_CALLABLE_H_

// Grace period is the time you need when receiving a lost lock callback to cleanup
// You are safe during this time, but you'll have to abort anything taking longer!!
#include "TimeDurationType.h"


namespace youtils
{

DECLARE_DURATION_TYPE(GracePeriod);

class GlobalLockedCallable
{
public:
    virtual ~GlobalLockedCallable()
    {}

    virtual const GracePeriod&
    grace_period() const = 0;

};

}

#endif // GLOBAL_LOCKED_CALLABLE_H_

// Local Variables: **
// mode: c++ **
// End: **
