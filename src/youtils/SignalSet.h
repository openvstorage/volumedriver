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
