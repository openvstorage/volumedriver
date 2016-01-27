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
