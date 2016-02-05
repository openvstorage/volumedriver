// Copyright 2016 iNuron NV
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

#ifndef VFS_STATS_COLLECTOR_COMPONENT_H_
#define VFS_STATS_COLLECTOR_COMPONENT_H_

#include "FileSystemParameters.h"
#include "StatsCollector.h"

#include <mutex>

#include <youtils/Logging.h>
#include <youtils/VolumeDriverComponent.h>

namespace volumedriverfs
{

class StatsCollectorComponent
    : public youtils::VolumeDriverComponent
{
public:
    StatsCollectorComponent(const boost::property_tree::ptree&,
                         const RegisterComponent);

    ~StatsCollectorComponent() = default;

        virtual const char*
    componentName() const override final
    {
        return initialized_params::stats_collector_component_name;
    }

    virtual void
    update(const boost::property_tree::ptree&,
           youtils::UpdateReport&) override final;

    virtual void
    persist(boost::property_tree::ptree&,
            const ReportDefault) const override final;

    virtual bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final;

private:
    DECLARE_LOGGER("VFSStatsCollectorComponent");

    DECLARE_PARAMETER(stats_collector_interval_secs);
    DECLARE_PARAMETER(stats_collector_destination);

    std::mutex lock_;
    std::unique_ptr<StatsCollector> stats_collector_;
};

}

#endif // VFS_STATS_COLLECTOR_COMPONENT_H_
