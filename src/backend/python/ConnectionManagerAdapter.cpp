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

#include "ConnectionManagerAdapter.h"
#include "InterfaceAdapter.h"

#include "../BackendConnectionManager.h"
#include "../BackendInterface.h"
#include "../Namespace.h"

#include <boost/python.hpp>
#include <boost/python/class.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/manage_new_object.hpp>
#include <boost/python/return_value_policy.hpp>

#include <youtils/ConfigFetcher.h>
#include <youtils/Uri.h>

#include <backend/BackendParameters.h>

namespace backend
{

namespace python
{

namespace bpy = boost::python;
namespace ypy = youtils::python;
namespace yt = youtils;

namespace
{

BackendConnectionManagerPtr
create_connection_manager(const std::string& uri)
{
    std::unique_ptr<yt::ConfigFetcher>
        fetcher(yt::ConfigFetcher::create(yt::Uri(uri)));
    return BackendConnectionManager::create((*fetcher)(VerifyConfig::F));
}

// boost::python cannot deal with unique_ptrs
BackendInterface*
create_backend_interface(BackendConnectionManager& cm,
                         const std::string& nspace)
{
    return cm.newBackendInterface(Namespace(nspace)).release();
}

}

DEFINE_PYTHON_WRAPPER(ConnectionManagerAdapter)
{
    ypy::register_once<InterfaceAdapter>();

    bpy::class_<BackendConnectionManager,
                BackendConnectionManagerPtr,
                boost::noncopyable>("BackendConnectionManager",
                                    bpy::no_init)
        .def("__init__",
             bpy::make_constructor(&create_connection_manager,
                                   bpy::default_call_policies(),
                                   (bpy::args("config_uri"))),
             "Create a ConnectionManager from a config\n"
             "@config_uri, string, where to find the config\n")
        .def("create_backend_interface",
             &create_backend_interface,
             bpy::return_value_policy<bpy::manage_new_object>(),
             (bpy::args("nspace")),
             "create BackendInterface for a given namespace\n"
             "@param nspace, string, namespace name\n")
        ;
}

}

}
