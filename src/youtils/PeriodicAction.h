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
                   Action&& action,
                   const std::atomic<uint64_t>& period,
                   const bool periodInSeconds = true,
                   const boost::chrono::milliseconds& ramp_up =
                   boost::chrono::milliseconds(0));

    PeriodicAction(const std::string& name,
                   AbortableAction&& action,
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
