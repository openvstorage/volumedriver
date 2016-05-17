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
                           Handler handler)
    : blocker_(sigset)
    , handler_(std::move(handler))
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
