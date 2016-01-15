// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
    read_config(std::stringstream&);

    static boost::property_tree::ptree
    read_config_file(const boost::filesystem::path&);

private:
    DECLARE_LOGGER("VolumeDriverComponent");

    const RegisterComponent registered_;
};

}

#endif // VOLUME_DRIVER_COMPONENT_H

// Local Variables: **
// mode: c++ **
// End: **
