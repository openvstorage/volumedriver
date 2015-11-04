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

#ifndef __SHM_ORB_INTERFACE_H_
#define __SHM_ORB_INTERFACE_H_

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <youtils/OrbHelper.h>

#include <youtils/VolumeDriverComponent.h>

namespace volumedriverfs
{

class ShmOrbInterface
    : public youtils::VolumeDriverComponent
{
public:
    ShmOrbInterface(const boost::property_tree::ptree& pt,
                    const RegisterComponent registerizle,
                    FileSystem& fs)
    : youtils::VolumeDriverComponent(registerizle,
                                pt)
    , shm_region_size(pt)
    , fs_(fs)
    , orb_helper_("volumedriverfs_shm_server")
    {}

    ~ShmOrbInterface() = default;

    ShmOrbInterface(const ShmOrbInterface&) = delete;

    ShmOrbInterface&
    operator=(const ShmOrbInterface&) = delete;

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

    void
    run();

    void
    stop_all_and_exit();

    size_t
    shm_size() const
    {
        return shm_region_size.value();
    }

private:
    DECLARE_LOGGER("ShmOrbInterface");

    DECLARE_PARAMETER(shm_region_size);

    FileSystem& fs_;
    youtils::OrbHelper orb_helper_;
    boost::interprocess::managed_shared_memory shm_segment_;
};

}

#endif
