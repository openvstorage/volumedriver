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

#ifndef CONFIGURATION_REPORT_H
#define CONFIGURATION_REPORT_H

#include "Serialization.h"

#include <list>
#include <string>

#include <boost/predef/version_number.h>
#include <boost/serialization/list.hpp>
#include <boost/version.hpp>

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

    // work around load_construct_data being broken in boost 1.58
    // https://svn.boost.org/trac/boost/ticket/11343
#if BOOST_VERSION == 105800
    ConfigurationProblem()
        : param_name_("uninitialized")
        , component_name_("uninitialized")
        , problem_("uninitialized")
    {}
#endif

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
