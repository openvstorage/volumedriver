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


#ifndef UPDATE_REPORT_H
#define UPDATE_REPORT_H

#include "DimensionedValue.h"
#include "Serialization.h"

#include <boost/lexical_cast.hpp>
#include <boost/serialization/list.hpp>

#include <list>
#include <string>
#include <sstream>

namespace youtils
{

struct ParameterUpdate
{
    template<typename T>
    ParameterUpdate(const std::string& name,
                    const T& old,
                    const T& new_)
        : parameter_name(name)
        , old_value(boost::lexical_cast<std::string>(old))
        , new_value(boost::lexical_cast<std::string>(new_))
    {}

    ~ParameterUpdate() = default;

    ParameterUpdate(const ParameterUpdate&) = default;

    ParameterUpdate&
    operator=(const ParameterUpdate& other)
    {
        if (this != &other)
        {
            const_cast<std::string&>(parameter_name) = other.parameter_name;
            const_cast<std::string&>(old_value) = other.old_value;
            const_cast<std::string&>(new_value) = other.new_value;
        }

        return *this;
    }

    // really only there for testers
    bool
    operator==(const ParameterUpdate& other) const
    {
        return parameter_name == other.parameter_name and
            old_value == other.old_value and
            new_value == other.new_value;
    }

    bool
    operator!=(const ParameterUpdate& other) const
    {
        return not (*this == other);
    }

    template<typename Archive>
    void
    serialize(Archive& ar, unsigned version)
    {
        CHECK_VERSION(version, 1);

        ar & boost::serialization::make_nvp("parameter_name",
                                            const_cast<std::string&>(parameter_name));
        ar & boost::serialization::make_nvp("old_value",
                                            const_cast<std::string&>(old_value));
        ar & boost::serialization::make_nvp("new_value",
                                            const_cast<std::string&>(new_value));
    }

    const std::string parameter_name;
    const std::string old_value;
    const std::string new_value;
};

class UpdateReport
{
public:
    UpdateReport() = default;

    ~UpdateReport() = default;

    UpdateReport(const UpdateReport&) = default;

    UpdateReport&
    operator=(const UpdateReport&) = default;

    template<typename T>
    void
    update(const char* name,
           const T& old,
           const T& new_)
    {
        updates.emplace_back(name, old, new_);
    }

    template<typename T>
    void
    no_update(const char* name,
              const T& old,
              const T& new_)
    {
        no_updates.emplace_back(name, old, new_);
    }

    void
    no_update(const char* name)
    {
        no_updates.emplace_back(name,
                                std::string(),
                                std::string());
    }

    uint32_t
    update_size() const
    {
        return updates.size();
    }

    uint32_t
    no_update_size() const
    {
        return no_updates.size();
    }

    std::list<ParameterUpdate>
    getUpdates() const
    {
        return updates;
    }

    std::list<ParameterUpdate>
    getNoUpdates() const
    {
        return no_updates;
    }

    // really only there for testers
    bool
    operator==(const UpdateReport& other) const
    {
        return updates == other.updates and
            no_updates == other.no_updates;
    }

    bool
    operator!=(const UpdateReport& other) const
    {
        return not (*this == other);
    }

private:
    std::list<ParameterUpdate> updates;
    std::list<ParameterUpdate> no_updates;

    friend class boost::serialization::access;

    template<typename Archive>
    void
    serialize(Archive& ar, unsigned version)
    {
        CHECK_VERSION(version, 1);

        ar & boost::serialization::make_nvp("updates", updates);
        ar & boost::serialization::make_nvp("no_updates", no_updates);
    }
};

}

BOOST_CLASS_VERSION(youtils::ParameterUpdate, 1);
BOOST_CLASS_VERSION(youtils::UpdateReport, 1);

namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
load_construct_data(Archive&,
                    youtils::ParameterUpdate* update,
                    const unsigned int /* version */)
{
    new(update) youtils::ParameterUpdate("uninitialized",
                                         "uninitialized",
                                         "uninitialized");
}

}

}

#endif // UPDATE_REPORT_H

// Local Variables: **
// mode: c++ **
// End: **
