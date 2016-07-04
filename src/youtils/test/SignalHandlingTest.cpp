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

#include "../SignalBlocker.h"
#include "../SignalSet.h"
#include "../SignalThread.h"
#include <gtest/gtest.h>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace youtilstest
{

namespace yt = youtils;

class SignalHandlingTest
    : public testing::Test
{
protected:
    DECLARE_LOGGER("SignalHandlingTest");
};

TEST_F(SignalHandlingTest, signal_thread)
{
    const yt::SignalSet sigset{ SIGUSR1, SIGUSR2 };
    std::set<int> signals_seen;

    yt::SignalBlocker blocker(sigset);

    const pid_t pid = getpid();

    int ret = kill(pid,
                   SIGUSR2);
    ASSERT_EQ(0, ret);

    ret = kill(pid,
               SIGUSR1);
    ASSERT_EQ(0, ret);

    std::mutex lock;
    std::unique_lock<decltype(lock)> u(lock);
    std::condition_variable cond;

    yt::SignalThread sigthread(sigset,
                               [&](int signal)
                               {
                                   std::lock_guard<decltype(lock)> u(lock);
                                   signals_seen.insert(signal);
                                   cond.notify_one();
                               });

    cond.wait(u,
              [&]() -> bool
              {
                  return signals_seen.size() == 2;
              });

    ASSERT_TRUE(signals_seen.find(SIGUSR1) != signals_seen.end());
    ASSERT_TRUE(signals_seen.find(SIGUSR2) != signals_seen.end());
}

}
