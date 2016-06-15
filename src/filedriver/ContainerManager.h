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
#include <youtils/IOException.h>
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
    MAKE_EXCEPTION(Exception, fungi::IOException);
    MAKE_EXCEPTION(ContainerDoesNotExistException, Exception);
    MAKE_EXCEPTION(ContainerExistsAlreadyException, Exception);

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
