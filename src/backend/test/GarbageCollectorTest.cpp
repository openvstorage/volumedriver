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

#include "BackendTestBase.h"

#include "../GarbageCollector.h"

#include <boost/property_tree/ptree.hpp>

namespace backendtest
{

using namespace backend;

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace yt = youtils;

class GarbageCollectorTest
    : public BackendTestBase
{
protected:
    GarbageCollectorTest()
        : BackendTestBase("GarbageCollectorTest")
    {}

    void
    SetUp() override final
    {
        BackendTestBase::SetUp();
        bpt::ptree pt;
        gc_ = std::make_unique<GarbageCollector>(cm_,
                                                 pt,
                                                 RegisterComponent::F);
    }

    void
    TearDown() override final
    {
        gc_ = nullptr;
        BackendTestBase::TearDown();
    }

    GarbageCollector::ThreadPool&
    thread_pool(GarbageCollector& gc)
    {
        return gc.thread_pool_;
    }

    std::unique_ptr<GarbageCollector> gc_;
};

TEST_F(GarbageCollectorTest, inexistent_namespace)
{
    Namespace nspace;

    ASSERT_FALSE(cm_->getConnection()->namespaceExists(nspace));

    gc_->queue(Garbage(nspace,
                       { "some_object" }));

    EXPECT_TRUE(gc_->barrier(nspace).get());
}

TEST_F(GarbageCollectorTest, inexistent_object)
{
    auto wrns(make_random_namespace());

    const std::string oname("some-object");
    ASSERT_TRUE(cm_->getConnection()->namespaceExists(wrns->ns()));
    ASSERT_FALSE(cm_->getConnection()->objectExists(wrns->ns(),
                                                    oname));

    gc_->queue(Garbage(wrns->ns(),
                       { oname }));

    EXPECT_TRUE(gc_->barrier(wrns->ns()).get());

    EXPECT_FALSE(cm_->getConnection()->objectExists(wrns->ns(),
                                                    oname));
}

TEST_F(GarbageCollectorTest, empty_garbage)
{
    auto wrns(make_random_namespace());

    ASSERT_TRUE(cm_->getConnection()->namespaceExists(wrns->ns()));

    gc_->queue(Garbage());

    EXPECT_TRUE(gc_->barrier(wrns->ns()).get());
}

TEST_F(GarbageCollectorTest, real_garbage)
{
    const size_t object_size = 4096;
    const size_t object_count = 13;

    std::vector<std::string> objects;
    objects.reserve(object_count);

    auto wrns(make_random_namespace());

    auto check_object_count([&](size_t exp)
                            {
                                std::list<std::string> l;
                                cm_->getConnection()->listObjects(wrns->ns(),
                                                                  l);
                                EXPECT_EQ(exp,
                                          l.size());
                            });

    check_object_count(0);

    for (size_t i = 0; i < object_count; ++i)
    {
        auto s(boost::lexical_cast<std::string>(i));
        const fs::path p(path_ / s);

        createAndPut(p,
                     object_size,
                     s,
                     cm_,
                     s,
                     wrns->ns(),
                     OverwriteObject::F);

        objects.emplace_back(s);
    }

    check_object_count(object_count);

    gc_->queue(Garbage(wrns->ns(),
                       objects));

    EXPECT_TRUE(gc_->barrier(wrns->ns()).get());

    check_object_count(0);
}

namespace
{

struct ThrowingTask
    : public GarbageCollector::ThreadPool::Task
{
    ThrowingTask(const Namespace& ns)
        : GarbageCollector::ThreadPool::Task(yt::BarrierTask::T)
        , nspace(ns.str())
    {}

    ~ThrowingTask() = default;

    void
    run(int /* thread id */) override final
    {
        throw std::runtime_error("throwing an exception is my sole purpose");
    }

    const std::string&
    getName() const override final
    {
        static const std::string s("ThrowingTask");
        return s;
    }

    const std::string&
    getProducerID() const override final
    {
        return nspace;
    }

    std::string nspace;
};

}

TEST_F(GarbageCollectorTest, blocked)
{
    const size_t object_size = 4096;

    const std::string name("some-object");

    auto wrns(make_random_namespace());

    ASSERT_FALSE(cm_->getConnection()->objectExists(wrns->ns(),
                                                    name));
    const fs::path p(path_ / name);
    createAndPut(p,
                 object_size,
                 name,
                 cm_,
                 name,
                 wrns->ns(),
                 OverwriteObject::F);
    ASSERT_TRUE(cm_->getConnection()->objectExists(wrns->ns(),
                                                   name));

    auto t(std::make_unique<ThrowingTask>(wrns->ns()));
    thread_pool(*gc_).addTask(std::move(t));

    std::future<bool> f(gc_->barrier(wrns->ns()));

    gc_->queue(Garbage(wrns->ns(),
                       { name }));

    gc_ = nullptr;

    EXPECT_FALSE(f.get());

    EXPECT_TRUE(cm_->getConnection()->objectExists(wrns->ns(),
                                                   name));
}

}
