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
