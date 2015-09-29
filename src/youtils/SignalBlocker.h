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

#ifndef YT_SIGNAL_BLOCKER_H_
#define YT_SIGNAL_BLOCKER_H_

#include "IOException.h"
#include "Logging.h"
#include "SignalSet.h"

#include <signal.h>
#include <string.h>

namespace youtils
{

template<bool block>
class SignalMasker
{
public:
    MAKE_EXCEPTION(Exception,
                   fungi::IOException);

    explicit SignalMasker(const SignalSet& set)
    {
        int ret = pthread_sigmask(block ?
                                  SIG_BLOCK :
                                  SIG_UNBLOCK,
                                  &set.sigset(),
                                  &stored_);
        if (ret)
        {
            LOG_ERROR("Failed to " << (block ? "block" : "unblock") << " signals: " <<
                      strerror(errno));
            throw Exception("Failed to modify sigmask");
        }
    }

    ~SignalMasker()
    {
        int ret = pthread_sigmask(SIG_SETMASK,
                                  &stored_,
                                  nullptr);
        if (ret)
        {
            LOG_ERROR("Failed to restore sigmask: " << strerror(errno));
        }
    }

    SignalMasker(const SignalMasker&) = delete;

    SignalMasker&
    operator=(const SignalMasker&) = delete;

private:
    DECLARE_LOGGER("SignalMasker");

    sigset_t stored_;
};

using SignalBlocker = SignalMasker<true>;
using SignalUnBlocker = SignalMasker<false>;

}

#endif // !YT_SIGNAL_BLOCKER_H_
