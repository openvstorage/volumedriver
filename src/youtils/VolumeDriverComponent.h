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

#ifndef VOLUME_DRIVER_COMPONENT_H
#define VOLUME_DRIVER_COMPONENT_H

#include "BooleanEnum.h"
#include "ConfigurationReport.h"
#include "InitializedParam.h"
#include "Logging.h"
#include "UpdateReport.h"

#include <exception>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

BOOLEAN_ENUM(RegisterComponent);
BOOLEAN_ENUM(VerifyConfig);

namespace youtils
{

class VolumeDriverComponent
{
    // Just to make sure you make a constructor from a property tree
protected:
    VolumeDriverComponent(const RegisterComponent,
                          const boost::property_tree::ptree& pt);

    virtual ~VolumeDriverComponent();

public:
    VolumeDriverComponent() = delete;

    VolumeDriverComponent(const VolumeDriverComponent&) = default;

    VolumeDriverComponent&
    operator=(const VolumeDriverComponent&) = default;

    virtual const char*
    componentName() const = 0;

    virtual void
    update(const boost::property_tree::ptree&,
           UpdateReport& report) = 0;

    virtual void
    persist(boost::property_tree::ptree&,
            const ReportDefault) const = 0;

    virtual bool
    checkConfig(const boost::property_tree::ptree&,
                ConfigurationReport&) const = 0;

    static void
    verify_property_tree(const boost::property_tree::ptree&);

    static boost::property_tree::ptree
    read_config(std::stringstream&,
                VerifyConfig);

    static boost::property_tree::ptree
    read_config_file(const boost::filesystem::path&,
                     VerifyConfig);

private:
    DECLARE_LOGGER("VolumeDriverComponent");

    const RegisterComponent registered_;
};

}

#endif // VOLUME_DRIVER_COMPONENT_H

// Local Variables: **
// mode: c++ **
// End: **
