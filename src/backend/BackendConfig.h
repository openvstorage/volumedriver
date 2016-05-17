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

#ifndef BACKEND_CONFIG_H
#define BACKEND_CONFIG_H

#include "BackendParameters.h"

#include <memory>

#include <boost/serialization/export.hpp>

#include <youtils/BooleanEnum.h>
#include <youtils/Logging.h>
#include <youtils/ConfigurationReport.h>
#include <youtils/UpdateReport.h>


namespace backend
{

BOOLEAN_ENUM(UseSSL);
BOOLEAN_ENUM(SSLVerifyHost);
BOOLEAN_ENUM(StrictConsistency);

class BackendConfig
{
    friend class boost::serialization::access;

public:
    static std::unique_ptr<BackendConfig>
    makeBackendConfig(const boost::property_tree::ptree&);

    static std::unique_ptr<BackendConfig>
    makeBackendConfig(const std::string& backend_config);

    static std::unique_ptr<BackendConfig>
    makeBackendConfig(const boost::filesystem::path& json_file);


    virtual std::unique_ptr<BackendConfig>
    clone() const = 0;

    virtual ~BackendConfig()
    {}

    virtual bool
    operator==(const BackendConfig& cfg) const = 0;

    virtual void
    persist_internal(boost::property_tree::ptree&,
                     const ReportDefault) const = 0;

    virtual void
    update_internal(const boost::property_tree::ptree&,
                    youtils::UpdateReport&) = 0;

    virtual bool
    checkConfig_internal(const boost::property_tree::ptree&,
                         youtils::ConfigurationReport&) const = 0;

    virtual const char*
    name() const = 0;

    virtual StrictConsistency
    strict_consistency() const = 0;

    virtual const char*
    componentName() const;

    DECLARE_PARAMETER(backend_type);

protected:
    DECLARE_LOGGER("BackendConfig");

    BackendConfig(BackendType tp)
        : backend_type(tp)
    {}

    BackendConfig(const boost::property_tree::ptree& pt)
        : backend_type(pt)
    {}

    BackendConfig() = delete;

    BackendConfig(const BackendConfig&) = delete;

    BackendConfig&
    operator=(const BackendConfig&) = delete;

    void
    putBackendType(boost::property_tree::ptree& pt,
                   const ReportDefault reportDefault) const
    {
        backend_type.persist(pt,
                             reportDefault);
    }

    static std::string
    sectionName(const std::string& section);
};

}

#endif // BACKEND_CONFIG_H_

// Local Variables: **
// mode: c++ **
// End: **
