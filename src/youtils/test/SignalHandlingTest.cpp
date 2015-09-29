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

#include "../SignalBlocker.h"
#include "../SignalSet.h"
#include "../SignalThread.h"
#include "../TestBase.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace youtilstest
{

namespace yt = youtils;

class SignalHandlingTest
    : public TestBase
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
