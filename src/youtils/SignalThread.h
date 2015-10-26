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
