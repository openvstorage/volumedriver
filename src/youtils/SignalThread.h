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

#ifndef YT_SIGNAL_THREAD_H_
#define YT_SIGNAL_THREAD_H_

#include "IOException.h"
#include "Logging.h"
#include "SignalBlocker.h"
#include "SignalSet.h"

#include <functional>
#include <set>

#include <signal.h>

#include <boost/asio.hpp>
#include <boost/thread.hpp>

namespace youtils
{

class SignalSet;

class SignalThread
{
public:
    MAKE_EXCEPTION(Exception,
                   fungi::IOException);

    using Handler = std::function<void(int signal)>;

    SignalThread(const SignalSet& sigset,
                  Handler handler);

    ~SignalThread();

    SignalThread(const SignalThread&) = delete;

    SignalThread&
    operator=(const SignalThread&) = delete;

private:
    DECLARE_LOGGER("SignalThread");

    SignalBlocker blocker_;
    Handler handler_;
    boost::asio::io_service io_service_;
    boost::thread thread_;

    void
    run_(const SignalSet& sigset);
};

}

#endif // YT_SIGNAL_THREAD_H_
