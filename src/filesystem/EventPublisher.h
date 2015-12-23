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
