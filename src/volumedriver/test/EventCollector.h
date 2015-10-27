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
