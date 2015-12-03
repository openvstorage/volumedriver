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

#include "BackendException.h"
#include "BackendInterface.h"
#include "GarbageCollector.h"

#include <boost/property_tree/ptree.hpp>

namespace backend
{

namespace bpt = boost::property_tree;
namespace yt = youtils;

const char*
GarbageCollectorThreadPoolTraits::component_name = GarbageCollector::name();

GarbageCollector::GarbageCollector(BackendConnectionManagerPtr cm,
                                   const bpt::ptree& pt,
                                   const RegisterComponent registerize)
    : cm_(cm)
    , thread_pool_(pt,
                   registerize)
{
}

namespace
{

DECLARE_LOGGER("GarbageCollectorUtils");

struct BarrierTask
    : public GarbageCollector::ThreadPool::Task
{
    BarrierTask(const std::string& n)
        : GarbageCollector::ThreadPool::Task(yt::BarrierTask::T)
        , nspace(n)
    {}

    ~BarrierTask()
    {
        try
        {
            promise.set_value(done);
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to fulfil promise");
    }

    void
    run(int /* thread id */) override final
    {
        done = true;
    }

    const std::string&
    getName() const override final
    {
        static const std::string s("GarbageCollectorBarrierTask");
        return s;
    }

    const std::string&
    getProducerID() const override final
    {
        return nspace;
    }

    bool done = false;
    std::promise<bool> promise;
    std::string nspace;
};

struct DeleteObjectTask
    : public GarbageCollector::ThreadPool::Task
{
    DeleteObjectTask(BackendConnectionManagerPtr c,
                     const std::string& n,
                     std::string o)
        : GarbageCollector::ThreadPool::Task(yt::BarrierTask::F)
        , cm(c)
        , nspace(n)
        , object_name(std::move(o))
    {}

    virtual ~DeleteObjectTask() = default;

    void
    run(int /* thread id */) override final
    {
        try
        {
            cm->newBackendInterface(Namespace(nspace))->remove(object_name,
                                                               ObjectMayNotExist::T);
        }
        catch (BackendNamespaceDoesNotExistException&)
        {
            LOG_ERROR(nspace << "/" << object_name <<
                      ": namespace does not exist (anymore?) - ignoring it");
        }
    }

    const std::string&
    getName() const override final
    {
        static const std::string s("GarbageCollectorDeleteObjectTask");
        return s;
    }

    const std::string&
    getProducerID() const override final
    {
        return nspace;
    }

    BackendConnectionManagerPtr cm;
    const std::string nspace;
    const std::string object_name;
};

}

void
GarbageCollector::queue(Garbage garbage)
{
    for (auto&& g : garbage.object_names)
    {
        std::unique_ptr<ThreadPool::Task> t(new DeleteObjectTask(cm_,
                                                                 garbage.nspace.str(),
                                                                 std::move(g)));
        thread_pool_.addTask(std::move(t));
    }
}

std::future<bool>
GarbageCollector::barrier(const Namespace& nspace)
{
    std::unique_ptr<BarrierTask> t(new BarrierTask(nspace.str()));
    auto f(t->promise.get_future());

    thread_pool_.addTask(std::move(t));
    return f;
}

const char*
GarbageCollector::name()
{
    return "backend_garbage_collector";
}

}
