// Copyright 2015 Open vStorage NV
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

#ifndef PYTHON_BACKEND_CONNECTION_MANAGER_H
#define PYTHON_BACKEND_CONNECTION_MANAGER_H

#include <boost/python.hpp>
#include <boost/python/dict.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include <backend/BackendConnectionManager.h>

namespace pythonbackend
{

class ConnectionInterface;

class ConnectionManager
{
public:
    // Have an initializer from a path & one from a string
    ConnectionManager(const boost::python::dict& backend_config);

    ConnectionManager(const std::string& config_file);

    ConnectionManager&
    operator=(const ConnectionManager&) = delete;

    ConnectionManager() = delete;

    ConnectionInterface*
    getConnection();

    std::string
    str() const;

    std::string
    repr() const;

public:
    backend::BackendConnectionManagerPtr backend_conn_manager_;

    void
    _init(const boost::property_tree::ptree& pt);

    boost::property_tree::ptree root;
};

}

#endif // PYTHON_BACKEND_CONNECTION_MANAGER_H

// Local Variables: **
// mode: c++ **
// End: **
