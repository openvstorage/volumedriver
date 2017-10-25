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

#include <gtest/gtest.h>
#include "../Generator.h"

namespace youtilstest
{

using namespace youtils;

class ThreadedGeneratorTest
    : public testing::Test
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

template<typename Generator>
std::vector<typename Generator::Item>
into_vector(Generator& gen)
{
    std::vector<typename Generator::Item> vec;
    while (not gen.finished())
    {
        vec.push_back(std::move(gen.current()));
        gen.next();
    }

    return vec;
}

class IntGen
    : public Generator<int>
{
public:
    explicit IntGen(int maxCount)
        : maxCount_(maxCount)
        , i_(0)
    {}

    void
    next() override final
    {
        i_++;
    }

    bool
    finished() override final
    {
        return i_ >= maxCount_;
    }

    int&
    current() override final
    {
        return i_;
    }

private:
    int maxCount_;
    int i_;
};

using IntPtr = std::unique_ptr<int>;

class IntPtrGen
    : public Generator<IntPtr>
{
public:
    explicit IntPtrGen(int maxCount)
        : maxCount_(maxCount)
        , pi_(std::make_unique<int>(i_))
    {}

    void
    next() override final
    {
        i_ += 1;
        pi_ = std::make_unique<int>(i_);
    }

    bool
    finished() override final
    {
        return i_ >= maxCount_;
    }

    IntPtr&
    current() override final
    {
        return pi_;
    }

private:
    int i_ = 0;
    int maxCount_;
    IntPtr pi_;
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
    explicit ThrowingIntGen(float prob = 0.05)
        : prob_(prob)
        , i_(0)
    {}

    void
    next() override final
    {
        maybeThrow();
        i_++;
    }

    bool
    finished() override final
    {
        maybeThrow();
        return false;
    }

    int& current() override final
    {
        maybeThrow();
        return i_;
    }

private:
    void maybeThrow()
    {
        if(drand48() < prob_)
        {
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
    ThreadedGenerator<IntPtr> g2(std::unique_ptr<IntPtrGen>(new IntPtrGen(n)), 10);
    const int g2_size = into_vector(g2).size();
    EXPECT_EQ(n, g2_size);
}

TEST_F(ThreadedGeneratorTest, test1)
{
    const unsigned n = 100;
    IntGen g1(n);
    ThreadedGenerator<int> g2(std::unique_ptr<IntGen>(new IntGen(n)), 10);
    std::vector<int> g1_vec(into_vector(g1));

    EXPECT_EQ(n, g1_vec.size());
    EXPECT_EQ(g1_vec,
              into_vector(g2));
}

TEST_F(ThreadedGeneratorTest, test2)
{
    const unsigned n = 100;
    IntGen g1(n);
    ThreadedGenerator<int> g2(std::unique_ptr<IntGen>(new IntGen(n)), 10);
    std::vector<int> g1_vec(into_vector(g1));

    EXPECT_EQ(n, g1_vec.size());
    EXPECT_EQ(g1_vec,
              into_vector(g2));
}

TEST_F(ThreadedGeneratorTest, testThrowingProducer)
{
    EXPECT_THROW({
            ThreadedGenerator<int>
                g(std::unique_ptr<ThrowingIntGen>(new ThrowingIntGen(1.0)), 10);
            into_vector(g);
        },
        std::exception);
}

struct PrefetchGeneratorTest
    : public testing::Test
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
