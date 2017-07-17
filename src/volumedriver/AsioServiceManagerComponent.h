// Copyright (C) 2017 iNuron NV
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

#ifndef VD_ASIO_SERVICE_MANAGER_COMPONENT_H_
#define VD_ASIO_SERVICE_MANAGER_COMPONENT_H_

#include "VolumeDriverParameters.h"

#include <boost/property_tree/ptree_fwd.hpp>

#include <youtils/ConfigurationReport.h>
#include <youtils/VolumeDriverComponent.h>

namespace youtils
{

class AsioServiceManager;
class UpdateReport;

}

namespace volumedriver
{

class AsioServiceManagerComponent
    : public youtils::VolumeDriverComponent
{
public:
    explicit AsioServiceManagerComponent(const boost::property_tree::ptree&,
                                         const RegisterComponent = RegisterComponent::F);

    ~AsioServiceManagerComponent() = default;

    AsioServiceManagerComponent(const AsioServiceManagerComponent&) = delete;

    AsioServiceManagerComponent&
    operator=(const AsioServiceManagerComponent&) = delete;

    std::shared_ptr<youtils::AsioServiceManager>
    asio_service_manager()
    {
        return manager_;
    }

    static const char*
    name()
    {
        return "asio_service_manager";
    }

    // VolumeDriverComponent Interface
    void
    persist(boost::property_tree::ptree&,
            const ReportDefault = ReportDefault::F) const override final;

    const char*
    componentName() const override final
    {
        return name();
    }

    void
    update(const boost::property_tree::ptree&,
           youtils::UpdateReport&) override final;

    bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final;

private:
    DECLARE_LOGGER("AsioServiceManagerComponent");

    DECLARE_PARAMETER(asio_service_manager_threads);
    DECLARE_PARAMETER(asio_service_manager_io_service_per_thread);

    std::shared_ptr<youtils::AsioServiceManager> manager_;
};

}

#endif // !VD_ASIO_SERVICE_MANAGER_COMPONENT_H_
