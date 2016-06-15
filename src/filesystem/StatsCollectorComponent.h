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
