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
