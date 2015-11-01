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

#include "ContainerManager.h"

#include <boost/property_tree/ptree.hpp>

#include <youtils/Catchers.h>
#include <youtils/IOException.h>
#include <youtils/FileDescriptor.h>

namespace filedriver
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace yt = youtils;

#define LOCK()                                                  \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

ContainerManager::ContainerManager(be::BackendConnectionManagerPtr cm,
                                   const bpt::ptree& pt,
                                   const RegisterComponent registrate)
    : VolumeDriverComponent(registrate,
                            pt)
    , fd_cache_path(pt)
    , fd_namespace(pt)
    , fd_extent_cache_capacity(pt)
    , bi_(cm->newBackendInterface(be::Namespace(fd_namespace.value())))
{
    if (not bi_->namespaceExists())
    {
        LOG_INFO("Namespace " << fd_namespace.value() << " does not exist - creating it");
        bi_->createNamespace();
    }

    extent_cache_ = std::make_shared<ExtentCache>(fd_cache_path.value(),
                                                  fd_extent_cache_capacity.value());

    LOG_INFO("Up and running, namespace " << fd_namespace.value());
}

void
ContainerManager::destroy(be::BackendConnectionManagerPtr cm,
                          const bpt::ptree& pt)
{
    const be::Namespace nspace(PARAMETER_VALUE_FROM_PROPERTY_TREE(fd_namespace,
                                                                  pt));

    LOG_INFO("Destroying filedriver namespace " << nspace);

    auto bi(cm->newBackendInterface(nspace));
    bi->deleteNamespace();
}

ContainerPtr
ContainerManager::find_locked_(const ContainerId& cid)
{
    LOG_TRACE(cid);

    auto it = containers_.find(cid);
    if (it != containers_.end())
    {
        return it->second;
    }
    else
    {
        return nullptr;
    }
}

ContainerPtr
ContainerManager::find_(const ContainerId& cid)
{
    LOG_TRACE(cid);

    LOCK();
    return find_locked_(cid);
}

ContainerPtr
ContainerManager::find_throw_(const ContainerId& cid)
{
    LOG_TRACE(cid);

    ContainerPtr c(find_(cid));
    if (c == nullptr)
    {
        LOG_ERROR("Container " << cid << " does not exist");
        throw fungi::IOException("Container does not exist",
                                 cid.c_str(),
                                 ENOENT);
    }

    return c;
}

void
ContainerManager::create(const ContainerId& cid)
{
    LOG_TRACE(cid);

    LOCK();

    if (find_locked_(cid))
    {
        LOG_ERROR("Cannot create " << cid << " as it exists already");
        throw fungi::IOException("Cannot create container as it exists already",
                                 cid.str().c_str(),
                                 EEXIST);
    }
    else
    {
        auto c(std::make_shared<Container>(cid,
                                           extent_cache_,
                                           bi_));
        containers_[cid] = c;
    }
}

size_t
ContainerManager::read(const ContainerId& cid,
                       off_t off,
                       void* buf,
                       size_t bufsize)
{
    LOG_TRACE(cid << ": off " << off << ", size " << bufsize);

    ContainerPtr c(find_throw_(cid));
    return c->read(off, buf, bufsize);
}

size_t
ContainerManager::write(const ContainerId& cid,
                        off_t off,
                        const void* buf,
                        size_t bufsize)
{
    LOG_TRACE(cid << ": off " << off << ", size " << bufsize);

    ContainerPtr c(find_throw_(cid));
    return c->write(off, buf, bufsize);
}

void
ContainerManager::sync(const ContainerId& cid)
{
    LOG_TRACE(cid);
}

void
ContainerManager::resize(const ContainerId& cid,
                         uint64_t size)
{
    LOG_TRACE(cid << ": size " << size);

    ContainerPtr c(find_throw_(cid));
    c->resize(size);
}

uint64_t
ContainerManager::size(const ContainerId& cid)
{
    LOG_TRACE(cid);

    ContainerPtr c(find_throw_(cid));
    return c->size();
}

void
ContainerManager::unlink(const ContainerId& cid)
{
    LOG_TRACE(cid);

    ContainerPtr c;

    {
        LOCK();
        c = find_locked_(cid);
        if (c != nullptr)
        {
            containers_.erase(cid);
        }
    }

    if (c == nullptr)
    {
        LOG_ERROR("Container " << cid << " does not exist");
        throw fungi::IOException("Container does not exist",
                                 cid.c_str(),
                                 ENOENT);
    }

    c->unlink();
}

void
ContainerManager::drop_from_cache(const ContainerId& cid)
{
    LOG_TRACE(cid);

    ContainerPtr c(find_throw_(cid));
    c->drop_from_cache();
}

void
ContainerManager::restart(const ContainerId& cid)
{
    LOG_INFO(cid);

    ContainerPtr c;

    {
        LOCK();

        c = find_locked_(cid);
        if (c != nullptr)
        {
            LOG_INFO(cid << " is already up and running, nothing to restart");
            return;
        }

        c = std::make_shared<Container>(cid,
                                        extent_cache_,
                                        bi_);
    }

    c->restart();

    LOCK();

    if (find_locked_(cid))
    {
        LOG_INFO(cid << " is already up and running - some other thread beat us to it");
    }
    else
    {
        containers_[cid] = c;
    }
}

const char*
ContainerManager::componentName() const
{
    return ip::file_driver_component_name;
}

void
ContainerManager::update(const bpt::ptree& pt,
                         yt::UpdateReport& rep)
{
#define U(val)                                  \
    val.update(pt, rep)

    U(fd_cache_path);
    U(fd_namespace);

    U(fd_extent_cache_capacity);

    if (fd_extent_cache_capacity.value() != extent_cache_->capacity())
    {
        extent_cache_->capacity(fd_extent_cache_capacity.value());
    }

#undef U
}

void
ContainerManager::persist(bpt::ptree& pt,
                          const ReportDefault report_default) const
{
#define P(var)                                  \
    (var).persist(pt, report_default);

    P(fd_cache_path);
    P(fd_namespace);
    P(fd_extent_cache_capacity);

#undef P
}

bool
ContainerManager::checkConfig(const bpt::ptree& pt,
                              yt::ConfigurationReport& crep) const
{
    ip::PARAMETER_TYPE(fd_extent_cache_capacity) cap(pt);
    if (cap.value() == 0)
    {
        yt::ConfigurationProblem p(cap.name(),
                                   cap.section_name(),
                                   "fd_extent_cache_capacity must be > 0");
        crep.emplace_back(p);
        return false;
    }
    else
    {
        return true;
    }
}

}
