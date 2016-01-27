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

#ifndef CONFIGURATION_REPORT_H
#define CONFIGURATION_REPORT_H

#include "Serialization.h"

#include <list>
#include <string>

#include <boost/serialization/list.hpp>

namespace youtils
{

class ConfigurationProblem
{
public:
    ConfigurationProblem(const std::string& param_name,
                         const std::string& component_name,
                         const std::string& problem)
        : param_name_(param_name)
        , component_name_(component_name)
        , problem_(problem)
    {}

    template<typename T>
    ConfigurationProblem(const T& param,
                         const std::string& problem)
        : param_name_(param.name())
        , component_name_(param.section())
        , problem_(problem)
    {}

    ~ConfigurationProblem() = default;

    ConfigurationProblem(const ConfigurationProblem&) = default;

    ConfigurationProblem&
    operator=(const ConfigurationProblem& other)
    {
        if (this != &other)
        {
            const_cast<std::string&>(param_name_) = other.param_name_;
            const_cast<std::string&>(component_name_) = other.component_name_;
            const_cast<std::string&>(problem_) = other.problem_;
        }

        return *this;
    }

    // really only there for testers
    bool
    operator==(const ConfigurationProblem& other) const
    {
        return param_name_ == other.param_name_ and
            component_name_ == other.component_name_ and
            problem_ == other.problem_;
    }

    // really only there for testers
    bool
    operator!=(const ConfigurationProblem& other) const
    {
        return not (*this == other);
    }

    const std::string&
    param_name() const
    {
        return param_name_;
    }

    const std::string&
    component_name() const
    {
        return component_name_;
    }

    const std::string&
    problem() const
    {
        return problem_;
    }

private:
    const std::string param_name_;
    const std::string component_name_;
    const std::string problem_;

    friend class boost::serialization::access;

    template<typename Archive>
    void
    serialize(Archive& ar, unsigned version)
    {
        CHECK_VERSION(version, 1);

        ar & boost::serialization::make_nvp("parameter_name",
                                            const_cast<std::string&>(param_name_));
        ar & boost::serialization::make_nvp("component_name",
                                            const_cast<std::string&>(component_name_));
        ar & boost::serialization::make_nvp("problem",
                                            const_cast<std::string&>(problem_));
    }
};

typedef std::list<ConfigurationProblem> ConfigurationReport;

}

BOOST_CLASS_VERSION(youtils::ConfigurationProblem, 1);

namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
load_construct_data(Archive&,
                    youtils::ConfigurationProblem* problem,
                    const unsigned int /* version */)
{
    new(problem) youtils::ConfigurationProblem("uninitialized",
                                               "uninitialized",
                                               "uninitialized");
}

}

}

#endif // CONFIGURATION_REPORT_H
