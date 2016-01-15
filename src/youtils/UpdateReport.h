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
