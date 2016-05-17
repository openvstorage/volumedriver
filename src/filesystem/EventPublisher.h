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

#ifndef VFS_EVENT_PUBLISHER_H_
#define VFS_EVENT_PUBLISHER_H_

#include "ClusterId.h"
#include "FileSystemParameters.h"
#include "NodeId.h"

#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/Logging.h>

#include <volumedriver/Events.h>

namespace events
{

class Event;

}

namespace volumedriverfstest
{
class EventPublisherTest;
}

namespace volumedriverfs
{

class EventPublisher
    : public events::PublisherInterface
{
    friend class volumedriverfstest::EventPublisherTest;

public:
    explicit EventPublisher(std::unique_ptr<events::PublisherInterface>);

    virtual ~EventPublisher();

    EventPublisher(const EventPublisher&) = delete;

    EventPublisher&
    operator=(const EventPublisher&) = delete;

    virtual void
    publish(const events::Event& ev) noexcept override final;

    events::PublisherInterface&
    publisher()
    {
        return *publisher_;
    }

private:
    DECLARE_LOGGER("EventPublisher");

    std::unique_ptr<events::PublisherInterface> publisher_;
    mutable boost::mutex lock_;

    using EventPtr = std::unique_ptr<events::Event>;
    std::deque<EventPtr> queue_; // push back, pop front

    std::atomic<bool> stop_;
    boost::condition_variable cond_;
    boost::thread thread_;

    void
    run_();
};

}

#endif // !VFS_EVENT_PUBLISHER_H_
