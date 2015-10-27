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

#ifndef FILE_DRIVER_CONTAINER_MANAGER_H_
#define FILE_DRIVER_CONTAINER_MANAGER_H_

#include "Container.h"
#include "Extent.h"
#include "ExtentCache.h"
#include "FileDriverParameters.h"

#include <map>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/ConfigurationReport.h>
#include <youtils/Logging.h>
#include <youtils/UpdateReport.h>
#include <youtils/VolumeDriverComponent.h>

#include <backend/BackendConnectionManager.h>
#include <backend/BackendInterface.h>


namespace filedriver
{

class ContainerManager
    : public youtils::VolumeDriverComponent
{
public:
    ContainerManager(backend::BackendConnectionManagerPtr cm,
                     const boost::property_tree::ptree& pt,
                     const RegisterComponent registerize = RegisterComponent::T);

    virtual ~ContainerManager() = default;

    ContainerManager(const ContainerManager&) = delete;

    ContainerManager&
    operator=(const ContainerManager&) = delete;

    static void
    destroy(backend::BackendConnectionManagerPtr cm,
            const boost::property_tree::ptree& pt);

    void
    create(const ContainerId& cid);

    size_t
    read(const ContainerId& cid,
         off_t off,
         void* buf,
         size_t bufsize);

    size_t
    write(const ContainerId& cid,
          off_t off,
          const void* buf,
          size_t bufsize);

    void
    sync(const ContainerId& cid);

    void
    resize(const ContainerId& cid,
           uint64_t size);

    uint64_t
    size(const ContainerId& cid);

    void
    unlink(const ContainerId& cid);

    void
    drop_from_cache(const ContainerId& cid);

    void
    restart(const ContainerId& cid);

    virtual const char*
    componentName() const override final;

    virtual void
    update(const boost::property_tree::ptree& pt,
           youtils::UpdateReport& rep) override final;

    virtual void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault reportDefault) const override final;

    virtual bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final;

private:
    DECLARE_LOGGER("ContainerManager");

    mutable boost::mutex lock_;
    std::map<ContainerId, ContainerPtr> containers_;

    DECLARE_PARAMETER(fd_cache_path);
    DECLARE_PARAMETER(fd_namespace);
    DECLARE_PARAMETER(fd_extent_cache_capacity);

    std::shared_ptr<backend::BackendInterface> bi_;
    std::shared_ptr<ExtentCache> extent_cache_;

    ContainerPtr
    find_(const ContainerId& cid);

    ContainerPtr
    find_throw_(const ContainerId& cid);

    ContainerPtr
    find_locked_(const ContainerId& cid);
};

}

#endif // ! FILE_DRIVER_CONTAINER_MANAGER_H
