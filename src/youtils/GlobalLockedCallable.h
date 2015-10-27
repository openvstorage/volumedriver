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
template<typename Callable>
class GlobalLockedCallableWrapCallable : public GlobalLockedCallable
{
    public:
    GlobalLockedCallableWrapCallable(const GracePeriod& grace_period,
                                     Callable& callable,
                                     const char* info = 0)
        : callable_(callable)
        , grace_period_(grace_period)
        , info_(info)
    {}

    virtual const GracePeriod&
    grace_period() const
    {
        return grace_period_;
    }

    void
    operator()()
    {
        callable_.operator()();
    }

    virtual ~GlobalLockedCallableWrapCallable()
    {};

    std::string
    info_for_locked_run()
    {
        if(info_)
        {
            return std::string(info_);
        }
        else
        {
            return std::string("wrapper round something else");
        }
    }


private:
    Callable& callable_;
    GracePeriod grace_period_;
    const char* info_;

};


template<typename Callable,
         typename...Args>
class GlobalLockedCallableWrapConstructor : public GlobalLockedCallable
{
public:
    GlobalLockedCallableWrapConstructor(const GracePeriod& grace_period,
                                        Args... args)
        : callable_(std::forward<Args>(args)...)
        , grace_period_(grace_period)
    {}

    virtual const GracePeriod&
    grace_period() const
    {
        return grace_period_;
    }

    void
    operator()()
    {
        callable_.operator()();
    }

    virtual ~GlobalLockedCallableWrapConstructor()
    {};

private:
    Callable callable_;
    GracePeriod grace_period_;
};


}

#endif // GLOBAL_LOCKED_CALLABLE_H_

// Local Variables: **
// mode: c++ **
// End: **
