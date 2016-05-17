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
