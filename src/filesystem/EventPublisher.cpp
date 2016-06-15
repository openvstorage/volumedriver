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

#include "EventPublisher.h"
#include "FileSystemEvents.pb.h"

namespace volumedriverfs
{

EventPublisher::EventPublisher(std::unique_ptr<events::PublisherInterface> publisher)
    : publisher_(std::move(publisher))
    , stop_(false)
    , thread_(boost::bind(&EventPublisher::run_,
                          this))
{
    VERIFY(publisher_);
    LOG_INFO("Started");
}

EventPublisher::~EventPublisher()
{
    LOG_INFO("Stopping");

    stop_ = true;
    cond_.notify_all();
    thread_.join();

    if (not queue_.empty())
    {
        LOG_WARN("Dropping " << queue_.size() << " events!");
    }
}

void
EventPublisher::publish(const events::Event& ev) noexcept
{
    try
    {
        try
        {
            boost::lock_guard<decltype(lock_)> g(lock_);
            queue_.emplace_back(std::make_unique<events::Event>(ev));
            cond_.notify_all();
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to publish event");
    }
    catch (...)
    {
        // don't let anything escape (bad_alloc anyone?)
    }
}

void
EventPublisher::run_()
{
    while (not stop_)
    {
        EventPtr ev;

        {
            boost::unique_lock<decltype(lock_)> u(lock_);
            cond_.wait(u,
                       [&]
                       {
                           return not queue_.empty() or stop_;
                       });

            if (not queue_.empty())
            {
                ev = std::move(queue_.front());
                queue_.pop_front();
            }
        }

        if (ev)
        {
            ASSERT(publisher_);
            publisher_->publish(*ev);
        }
    }
}

}
