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

#ifndef PERIODICACTION_H_
#define PERIODICACTION_H_

#include "BooleanEnum.h"
#include "Logging.h"

#include <atomic>
#include <functional>

#include <boost/chrono.hpp>
#include <boost/thread.hpp>

namespace youtils
{

BOOLEAN_ENUM(PeriodicActionContinue);

class PeriodicAction
{
public:

    using Action = std::function<void()>;
    using AbortableAction = std::function<PeriodicActionContinue()>;

    // period is in milliseconds if periodInSeconds is false
    PeriodicAction(const std::string& name,
                   Action action,
                   const std::atomic<uint64_t>& period,
                   const bool periodInSeconds = true,
                   const boost::chrono::milliseconds& ramp_up =
                   boost::chrono::milliseconds(0));

    PeriodicAction(const std::string& name,
                   AbortableAction action,
                   const std::atomic<uint64_t>& period,
                   const bool periodInSeconds = true,
                   const boost::chrono::milliseconds& ramp_up =
                   boost::chrono::milliseconds(0));

    ~PeriodicAction();

    PeriodicAction(const PeriodicAction&) = delete;

    PeriodicAction&
    operator=(const PeriodicAction) = delete;

private:
    DECLARE_LOGGER("PeriodicAction");

    const std::string name_;
    const std::atomic<uint64_t>& period_;
    AbortableAction action_;
    const bool period_in_seconds_;
    bool stop_ = false;
    // boost::thread as we use interrupt()
    boost::thread thread_;

    inline uint32_t
    periodInMilliSeconds() const
    {
        return (period_in_seconds_ ? 1000 : 1) * period_.load();
    }

    void
    run(boost::chrono::milliseconds ramp_up);
};

}

#endif /* PERIODICACTION_H_ */

// Local Variables: **
// mode: c++ **
// End: **
