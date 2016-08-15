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

VD_BOOLEAN_ENUM(PeriodicActionContinue);

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
