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

#include "Catchers.h"
#include "DeferredFileRemover.h"
#include "ScopeExit.h"
#include "UUID.h"

namespace youtils
{

#define LOCK()                                  \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

namespace fs = boost::filesystem;

DeferredFileRemover::DeferredFileRemover(const fs::path& path)
    : path_(path)
    , state_(State::Work)
{
    fs::create_directories(path_);

    thread_ = boost::thread(boost::bind(&DeferredFileRemover::work_,
                                        this));
}

DeferredFileRemover::~DeferredFileRemover()
{
    try
    {
        {
            LOCK();
            state_ = State::Stop;
            cond_.notify_all();
        }

        thread_.join();
    }
    CATCH_STD_ALL_LOG_IGNORE(path_ << ": failed to stop instance");
}

void
DeferredFileRemover::schedule_removal(const fs::path& from)
{
    const fs::path to(path_ / UUID().str());
    fs::rename(from,
               to);

    LOCK();
    state_ = State::Work;
    cond_.notify_all();
}

void
DeferredFileRemover::work_()
{
    try
    {
        while (true)
        {
            boost::unique_lock<decltype(lock_)> u(lock_);
            switch (state_)
            {
            case State::Idle:
                {
                    caller_cond_.notify_all();
                    cond_.wait(u,
                               [&]
                               {
                                   return state_ != State::Idle;
                               });
                    break;
                }
            case State::Work:
                {
                    state_ = State::Idle;
                    u.unlock();

                    auto lock_again(make_scope_exit([&]
                                                    {
                                                        u.lock();
                                                    }));

                    collect_garbage_();
                    break;
                }
            case State::Stop:
                caller_cond_.notify_all();
                return;
            }
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(path_ << ": collector thread caught exception: " << EWHAT << " - exiting");
        });
}

void
DeferredFileRemover::collect_garbage_()
{
    const fs::directory_iterator end;
    for (fs::directory_iterator it(path_); it != end; ++it)
    {
        try
        {
            fs::remove(it->path());
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to remove " << it->path());
    }
}

void
DeferredFileRemover::wait_for_completion_()
{
    boost::unique_lock<decltype(lock_)> u(lock_);
    caller_cond_.wait(u,
                      [&]
                      {
                          return
                              state_ == State::Idle or
                              state_ == State::Stop;
                      });
}

}
