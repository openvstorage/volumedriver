// Copyright 2015 Open vStorage NV
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

#ifndef VFS_REGISTRY_H_
#define VFS_REGISTRY_H_

#include "FileSystemParameters.h"
#include "LockedArakoon.h"

#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/ArakoonInterface.h>
#include <youtils/InitializedParam.h>
#include <youtils/Logging.h>
#include <youtils/VolumeDriverComponent.h>

namespace volumedriverfs
{

class Registry final
    : public youtils::VolumeDriverComponent
    , public LockedArakoon
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
