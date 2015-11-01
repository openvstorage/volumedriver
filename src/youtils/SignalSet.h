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

#ifndef YT_SIGNAL_SET_H_
#define YT_SIGNAL_SET_H_

#include "IOException.h"
#include "Logging.h"

#include <initializer_list>

#include <signal.h>

namespace youtils
{

class SignalSet
{
public:
    MAKE_EXCEPTION(Exception,
                   fungi::IOException);

    explicit SignalSet(const std::initializer_list<int>& sigs);

    SignalSet();

    ~SignalSet() = default;

    SignalSet(const SignalSet&) = default;

    SignalSet&
    operator=(const SignalSet&) = default;

    static SignalSet
    make_filled_set();

    void
    fill();

    void
    insert(int sig);

    void
    erase(int sig);

    const sigset_t&
    sigset() const
    {
        return sigset_;
    }

private:
    DECLARE_LOGGER("SignalSet");

    sigset_t sigset_;
};

}

#endif // !YT_SIGNAL_SET_H_
