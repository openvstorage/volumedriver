// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
