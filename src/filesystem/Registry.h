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

#ifndef VFS_REGISTRY_H_
#define VFS_REGISTRY_H_

#include "FileSystemParameters.h"

#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/ArakoonInterface.h>
#include <youtils/InitializedParam.h>
#include <youtils/LockedArakoon.h>
#include <youtils/Logging.h>
#include <youtils/VolumeDriverComponent.h>

namespace volumedriverfs
{

class Registry final
    : public youtils::VolumeDriverComponent
    , public youtils::LockedArakoon
{
public:
    explicit Registry(const boost::property_tree::ptree& pt,
                      const RegisterComponent registrate = RegisterComponent::T);

    virtual ~Registry() = default;

    Registry(const Registry&) = delete;

    Registry&
    operator=(const Registry&) = delete;

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
    DECLARE_LOGGER("VFSRegistry");

    DECLARE_PARAMETER(vregistry_arakoon_cluster_id);
    DECLARE_PARAMETER(vregistry_arakoon_cluster_nodes);
    DECLARE_PARAMETER(vregistry_arakoon_timeout_ms);
};

}

#endif // !VFS_REGISTRY_H_
