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

#ifndef VOLUMEDRIVER_TEST_EVENT_COLLECTOR_H_
#define VOLUMEDRIVER_TEST_EVENT_COLLECTOR_H_

#include "../Events.h"
#include "../Events.pb.h"

#include <list>
#include <mutex>

#include <boost/optional.hpp>

namespace volumedrivertest
{

class EventCollector
    : public events::PublisherInterface
{
private:
    mutable std::mutex lock_;
    std::list<events::Event> events_;

public:
    EventCollector() = default;

    virtual ~EventCollector() = default;

#define LOCK()                                  \
    std::lock_guard<decltype(lock_)> g(lock_)

    virtual void
    publish(const events::Event& ev) noexcept override final
    {
        LOCK();
        events_.push_back(ev);
    }

    void
    clear()
    {
        LOCK();
        events_.clear();
    }

    boost::optional<events::Event>
    pop()
    {
        boost::optional<events::Event> ev;

        LOCK();
        if (not events_.empty())
        {
            ev = std::move(events_.front());
            events_.pop_front();
        }

        return ev;
    }

    size_t
    size() const
    {
        LOCK();
        return events_.size();
    }

#undef LOCK
};

}

#endif // !VOLUMEDRIVER_TEST_EVENT_COLLECTOR_H_
