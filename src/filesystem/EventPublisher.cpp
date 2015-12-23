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
