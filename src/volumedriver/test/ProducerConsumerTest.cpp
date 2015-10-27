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

#include "ExGTest.h"
#include <youtils/ProCon.h>
#include "boost/filesystem.hpp"
namespace volumedriver
{

class ProducerConsumerTest : public ExGTest
{};

class SimpleIntProducer : public yin::Producer<int>
{
public:
    SimpleIntProducer(int maxCount, yin::Channel<int>& ch, yin::Latch& l) :
        yin::Producer<int>(ch, l),
        index_(0),
        maxCount_(maxCount)
    {}

protected:
    int produce()
    {
        EXPECT_TRUE(unlimited() or index_ < maxCount_);
        return index_++;
    }

    bool cancel()
    {
        if (unlimited()) return false;
        bool cancel = index_ >= maxCount_;
        if (cancel) mayStop(true);
        return cancel;
    }

private:
    bool unlimited(){return maxCount_ < 0;}
    int index_;
    int maxCount_;
};


class SimpleIntConsumer : public yin::Consumer<int>
{
public:
    SimpleIntConsumer(int maxCount, yin::Channel<int>& ch, yin::Latch& l) :
        yin::Consumer<int>(ch, l),
        maxCount_(maxCount)
    {}
protected:
    void consume(int x)
    {
        EXPECT_TRUE(unlimited() or int(lst.size()) < maxCount_);
        lst.push_back(x);
    }

    bool cancel()
    {
        if (unlimited()) return false;
        bool cancel = int(lst.size()) >= maxCount_;
        if (cancel) mayStop(true);
        return cancel;
    }

public:
    bool unlimited(){return maxCount_ < 0;}
    int maxCount_;
    std::vector<int> lst;
};


TEST_F(ProducerConsumerTest , producerStops1)
{
    const unsigned n = 10;
    yin::Channel<int> ch;
    yin::Latch noLatch(true);
    SimpleIntProducer prod(n, ch, noLatch);
    SimpleIntConsumer cons(-1, ch, noLatch);

    boost::thread t(boost::ref(prod));

    cons();
    ASSERT_EQ(n, cons.lst.size());
    t.join();
}

TEST_F(ProducerConsumerTest , producerStops2)
{
    const unsigned n = 200;
    yin::Channel<int> ch;
    yin::Latch noLatch(true);
    SimpleIntProducer prod(n, ch, noLatch);
    SimpleIntConsumer cons(-1, ch, noLatch);

    boost::thread t(boost::ref(cons));

    prod();
    t.join();
    ASSERT_EQ(n, cons.lst.size());
}

TEST_F(ProducerConsumerTest , producerStops3)
{
    const unsigned n = 10;
    yin::Channel<int> ch;
    yin::Latch noLatch(true);
    SimpleIntProducer prod(n, ch, noLatch);
    SimpleIntConsumer cons(-1, ch, noLatch);

    boost::thread t_prod(boost::ref(prod));
    boost::thread t_cons(boost::ref(cons));

    t_cons.join();
    ASSERT_EQ(n, cons.lst.size());
    t_prod.join();
}



TEST_F(ProducerConsumerTest , consumerStops)
{
    const unsigned n = 200;
    yin::Channel<int> ch;
    yin::Latch noLatch(true);
    SimpleIntProducer prod(-1, ch, noLatch);
    SimpleIntConsumer cons(n, ch, noLatch);

    boost::thread t(boost::ref(prod));

    cons();
    ASSERT_EQ(n, cons.lst.size());
    t.join();
}
}
