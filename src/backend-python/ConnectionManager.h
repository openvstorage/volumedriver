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
