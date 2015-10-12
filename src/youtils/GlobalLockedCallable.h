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
