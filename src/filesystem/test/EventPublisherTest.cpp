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

#include "../EventPublisher.h"
#include "../FileSystemEvents.h"

#include <youtils/TestBase.h>

namespace volumedriverfstest
{

using namespace volumedriverfs;

class EventPublisherTest
    : public youtilstest::TestBase
{
protected:
    struct TestPublisher
        : public events::PublisherInterface
    {
        std::deque<EventPublisher::EventPtr> events_;

        virtual ~TestPublisher() = default;

        virtual void
        publish(const events::Event& ev) noexcept override final
        {
            events_.emplace_back(std::make_unique<events::Event>(ev));
        }
    };

    bool
    publisher_queue_empty(const EventPublisher& ep)
    {
        boost::lock_guard<decltype(EventPublisher::lock_)> g(ep.lock_);
        return ep.queue_.empty();
    }
};

TEST_F(EventPublisherTest, construction_and_destruction)
{
    auto p(std::make_unique<TestPublisher>());
    EventPublisher ep(std::move(p));
}

TEST_F(EventPublisherTest, publish)
{
    EventPublisher ep(std::make_unique<TestPublisher>());

    const size_t count = 1234;

    for (size_t i = 0; i < count; ++i)
    {
        ep.publish(FileSystemEvents::up_and_running(boost::lexical_cast<std::string>(i)));
    }

    while (not publisher_queue_empty(ep))
    {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
    }

    auto& test_publisher = dynamic_cast<TestPublisher&>(ep.publisher());

    size_t i = 0;
    for (const auto& e : test_publisher.events_)
    {
        ASSERT_TRUE(e->HasExtension(events::up_and_running));
        const auto ext(e->GetExtension(events::up_and_running));

        ASSERT_TRUE(ext.has_mountpoint());
        EXPECT_EQ(boost::lexical_cast<std::string>(i),
                  ext.mountpoint());

        ++i;
    }

    ASSERT_EQ(count,
              i);

}

}
