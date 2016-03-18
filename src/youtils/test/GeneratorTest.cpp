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

#include "../TestBase.h"
#include "../Generator.h"

namespace youtilstest
{

using namespace youtils;

class ThreadedGeneratorTest
    : public TestBase
{
    DECLARE_LOGGER("ThreadedGeneratorTest");

public:
    ThreadedGeneratorTest()
    {
        LOG_DEBUG("Constructor");
        srand48 ( time(NULL) );
    }
};

namespace
{

class IntGen
    : public Generator<int>
{
public:
    IntGen(int maxCount): maxCount_(maxCount), i_(0) {}
    void next()     { i_++; }
    bool finished() { return i_ >= maxCount_;}
    int& current()  { return i_;}

private:
    int maxCount_;
    int i_;
};

typedef boost::shared_ptr<int> intptr;

class IntPtrGen
    : public Generator<intptr>
{
public:
    IntPtrGen(int maxCount): maxCount_(maxCount), i_(new int(0)) {}

    void    next()        { (*i_)++; }
    bool    finished()    { return (*i_) >= maxCount_;}
    intptr& current()     { return i_;}

private:
    int maxCount_;
    intptr i_;
};

class IntGenException
    : public std::exception
{
public:
    virtual const char*
    what() const throw ()
    {
        return "IntGenException";
    }
};

class ThrowingIntGen
    : public Generator<int>
{
public:
    ThrowingIntGen(float prob = 0.05): prob_(prob), i_(0) {}

    void next()     { maybeThrow();     i_++;   }
    bool finished() { maybeThrow();     return false;}
    int& current()  { maybeThrow();     return i_; }

private:
    void maybeThrow(){
        if(drand48() < prob_) {
            throw IntGenException();
        }
    }
    float prob_;
    int i_;
};

}

TEST_F(ThreadedGeneratorTest, templatedTypes)
{
    const int n = 100;
    ThreadedGenerator<intptr> g2(std::unique_ptr<IntPtrGen>(new IntPtrGen(n)), 10);
    const int g2_size = g2.toList()->size();
    EXPECT_EQ(n, g2_size);
}

TEST_F(ThreadedGeneratorTest, test1)
{
    const unsigned n = 100;
    IntGen g1(n);
    ThreadedGenerator<int> g2(std::unique_ptr<IntGen>(new IntGen(n)), 10);
    std::unique_ptr<std::vector<int>> g1_list(g1.toList());

    //EXPECT_EQ(*g1_list, *(g2.toList()));
    EXPECT_EQ(n, g1_list->size());
}

TEST_F(ThreadedGeneratorTest, test2)
{
    const unsigned n = 100;
    IntGen g1(n);
    ThreadedGenerator<int> g2(std::unique_ptr<IntGen>(new IntGen(n)), 10);
    std::unique_ptr<std::vector<int>> g1_list(g1.toList());

    //EXPECT_EQ(*g1_list, *(g2.toList()));
    EXPECT_EQ(n, g1_list->size());
}

TEST_F(ThreadedGeneratorTest, testThrowingProducer)
{
    EXPECT_THROW({
            ThreadedGenerator<int>
                g(std::unique_ptr<ThrowingIntGen>(new ThrowingIntGen(1.0)), 10);
            g.toList();
        },
        std::exception);
}

struct PrefetchGeneratorTest
    : public TestBase
{};

TEST_F(PrefetchGeneratorTest, empty)
{
    PrefetchGenerator<int> g(std::make_unique<IntGen>(0));
    EXPECT_TRUE(g.finished());
    EXPECT_EQ(0, g.size());
}

TEST_F(PrefetchGeneratorTest, not_empty)
{
    const int count = 2048;
    PrefetchGenerator<int> g(std::make_unique<IntGen>(count));

    EXPECT_FALSE(g.finished());
    EXPECT_EQ(static_cast<size_t>(count),
              g.size());

    int exp = 0;
    while (not g.finished())
    {
        EXPECT_EQ(exp,
                  g.current());
        g.next();
        exp += 1;
    }

    EXPECT_EQ(exp,
              count);
}

}

// Local Variables: **
// mode: c++ **
// End: **
