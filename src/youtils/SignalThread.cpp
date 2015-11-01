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

#include "Assert.h"
#include "Catchers.h"
#include "ScopeExit.h"
#include "SignalThread.h"

#include <sys/signalfd.h>

namespace youtils
{

namespace ba = boost::asio;
namespace bs = boost::system;


SignalThread::SignalThread(const SignalSet& sigset,
                           Handler&& handler)
    : blocker_(sigset)
    , handler_(handler)
{
     thread_ = boost::thread([this, sigset]
                             {
                                 try
                                 {
                                     run_(sigset);
                                 }
                                 CATCH_STD_ALL_LOG_IGNORE("signal thread caught exception");
                             });
}

SignalThread::~SignalThread()
{
    io_service_.stop();

    try
    {
        thread_.join();
    }
    CATCH_STD_ALL_LOG_IGNORE("failed to join signal thread: " << EWHAT);
}

void
SignalThread::run_(const SignalSet& sigset)
{
    // boost::asio has support for signals via signal_set. It does however not
    // accept a sigset_t and neither can a sigset_t nor a ba::signal_set be inspected.

    int sigfd = signalfd(-1,
                         &sigset.sigset(),
                         SFD_NONBLOCK);
    if (sigfd < 0)
    {
        LOG_ERROR("Failed to create signalfd: " << strerror(errno));
        throw Exception("Failed to create signalfd");
    }

    auto exit(make_scope_exit([&]
                              {
                                  close(sigfd);
                              }));

    std::function<void()> g;

    auto f([&](const bs::error_code& ec,
               std::size_t bytes)
           {
               VERIFY(bytes == 0);

               if (ec)
               {
                   LOG_ERROR("error in signal handler: " <<
                             ec.message());
               }
               else
               {
                   signalfd_siginfo si;
                   ssize_t ret = ::read(sigfd,
                                        &si,
                                        sizeof(si));
                   if (ret < 0)
                   {
                       if (ret != EAGAIN)
                       {
                           ret = errno;
                           LOG_ERROR("failed to read siginfo from signalfd: " <<
                                     strerror(ret) << " (" << ret << ")");
                       }
                       g();
                   }
                   else if (ret != sizeof(si))
                   {
                       LOG_ERROR("read less (" << ret << " ) than expected (" <<
                                 sizeof(si) << ") from signalfd");
                   }
                   else
                   {
                       LOG_INFO("caught signal " << si.ssi_signo);
                       handler_(si.ssi_signo);
                       g();
                   }
               }
           });

    ba::posix::stream_descriptor sigdesc(io_service_,
                                         sigfd);

    g = [&]()
        {
            sigdesc.async_read_some(ba::null_buffers(),
                                    f);
        };

    g();

    bs::error_code ec;
    io_service_.run(ec);

    if (ec)
    {
        LOG_ERROR("error running I/O service of signal thread: " <<
                  ec.message());
    };

    LOG_INFO("exiting signal handler thread");
}

}
