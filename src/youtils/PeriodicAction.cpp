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
#include "PeriodicAction.h"

#include <iostream>
#include <thread>

namespace youtils
{

namespace bpt = boost::posix_time;

namespace
{

PeriodicAction::AbortableAction
make_abortable_action(PeriodicAction::Action action)
{
    auto a([act = std::move(action)]() mutable -> auto
           {
               act();
               return PeriodicActionContinue::T;
           });
    return a;
}

}

PeriodicAction::PeriodicAction(const std::string& name,
                               AbortableAction action,
                               const std::atomic<uint64_t>& period,
                               const bool period_in_seconds,
                               const boost::chrono::milliseconds& ramp_up)
try
    : name_(name)
    , period_(period)
    , action_(std::move(action))
    , period_in_seconds_(period_in_seconds)
    , thread_([ramp_up, this]
              {
                  run(ramp_up);
              })
{
    LOG_INFO("Started periodic action " << name_ << ", period: " << period <<
             (period_in_seconds ? " s" : " ms"));
}
CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Failed to create periodic action " << name << ": " << EWHAT);
    });

PeriodicAction::PeriodicAction(const std::string& name,
                               Action action,
                               const std::atomic<uint64_t>& period,
                               const bool period_in_seconds,
                               const boost::chrono::milliseconds& ramp_up)
    : PeriodicAction(name,
                     make_abortable_action(std::move(action)),
                     period,
                     period_in_seconds,
                     ramp_up)
{}

PeriodicAction::~PeriodicAction()
{
    stop_ = true;

    try
    {
        thread_.interrupt();
        thread_.join();
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to stop thread: " << e.what());
    }
    catch (...)
    {
        LOG_ERROR("Failed to stop thread: unknown exception");
    }
}

void
PeriodicAction::run(boost::chrono::milliseconds ramp_up)
{
    auto take_a_nap([&](const boost::chrono::milliseconds& msecs)
                    {
                        try
                        {
                            // uses boost as we want it to be interruptible
                            boost::this_thread::sleep_for(msecs);
                        }
                        catch (boost::thread_interrupted&)
                        {
                            LOG_TRACE(name_ << ": interrupted during ramp up");
                        }
                        CATCH_STD_ALL_LOG_IGNORE(name_ <<
                                                 ": caught exception while waiting for timer");
                    });

    take_a_nap(ramp_up);

    bpt::ptime start = bpt::second_clock::universal_time();

    while (not stop_)
    {
        bpt::ptime now = bpt::microsec_clock::universal_time();
        VERIFY(now >= start);

        bpt::time_duration elapsed = now - start;

        uint64_t wait = periodInMilliSeconds() > elapsed.total_milliseconds() ?
            periodInMilliSeconds() - elapsed.total_milliseconds() : 0 ;

        if (wait == 0)
        {
            start = now;

            try
            {
                const auto cont = action_();
                if (cont == PeriodicActionContinue::F)
                {
                    LOG_INFO(name_ << " asked to be terminated");
                    stop_ = true;
                }
            }
            CATCH_STD_ALL_LOG_IGNORE(name_ << ": running action caught error");
        }
        else
        {
            take_a_nap(boost::chrono::milliseconds(wait));
        }
    }

    LOG_INFO(name_ << ": exiting");
}

}

// Local Variables: **
// mode: c++ **
// End: **
