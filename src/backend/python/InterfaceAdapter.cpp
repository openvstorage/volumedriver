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

#include "InterfaceAdapter.h"
#include "Exceptions.h"

#include "../BackendInterface.h"
#include "../BackendConnectionManager.h"
#include "../BackendRequestParameters.h"

#include <boost/filesystem.hpp>
#include <boost/python.hpp>
#include <boost/python/class.hpp>
#include <boost/python/return_value_policy.hpp>

#include <youtils/ConfigFetcher.h>
#include <youtils/Uri.h>
#include <youtils/python/ChronoDurationConverter.h>
#include <youtils/python/IterableConverter.h>
#include <youtils/python/OptionalConverter.h>
#include <youtils/python/StringyConverter.h>

#include <backend/BackendParameters.h>

namespace backend
{

namespace python
{

namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace ypy = youtils::python;
namespace yt = youtils;

namespace
{

std::string
str(const BackendInterface& bi)
{
    return bi.getNS().str();
}

std::list<std::string>
list_objects(BackendInterface& bi, const BackendRequestParameters& params)
{
    std::list<std::string> l;
    bi.listObjects(l,
                   params);
    return l;
}

void
write_object(BackendInterface& bi,
             const fs::path& path,
             const std::string& name,
             const OverwriteObject& overwrite,
             const BackendRequestParameters& params)
{
    bi.write(path,
             name,
             overwrite,
             nullptr, // chksum
             nullptr, // condition
             params);
}

void
remove_object(BackendInterface& bi,
              const std::string& name,
              bool may_not_exist,
              const BackendRequestParameters& params)
{
    bi.remove(name,
              may_not_exist ? ObjectMayNotExist::T : ObjectMayNotExist::F,
              nullptr,
              params);
}

void
create_namespace(BackendInterface& bi,
                 const BackendRequestParameters& params)
{
    bi.createNamespace(NamespaceMustNotExist::T,
                       params);
}

}

DEFINE_PYTHON_WRAPPER(InterfaceAdapter)
{
    ypy::register_once<Exceptions>();

    REGISTER_CHRONO_DURATION_CONVERTER(boost::chrono::milliseconds);

    using StringList = std::list<std::string>;
    REGISTER_ITERABLE_CONVERTER(StringList);

    REGISTER_OPTIONAL_CONVERTER(boost::chrono::milliseconds);
    REGISTER_OPTIONAL_CONVERTER(double);
    REGISTER_OPTIONAL_CONVERTER(uint32_t);

    REGISTER_STRINGY_CONVERTER(fs::path);

#define MAKE_PYTHON_VD_BOOLEAN_ENUM(name, doc)  \
    bpy::enum_<name>(#name, doc)                \
        .value("F", name::F)                    \
        .value("T", name::T)

    MAKE_PYTHON_VD_BOOLEAN_ENUM(InsistOnLatestVersion,
                                "Whether to insist on getting the latest version of an object, values are T and F");
    MAKE_PYTHON_VD_BOOLEAN_ENUM(OverwriteObject,
                                "Whether to overwrite an existing object in the backend, values are T and F");

#define ADD_PROP_RW(t, prop)                                            \
    add_property(#prop,                                                 \
                 bpy::make_getter(&t::prop ## _,                        \
                                  bpy::return_value_policy<bpy::return_by_value>()), \
                 bpy::make_setter(&t::prop ## _))

    bpy::class_<BackendRequestParameters>("BackendRequestParameters",
                                          bpy::init<>("Default constructor"))
        .ADD_PROP_RW(BackendRequestParameters, retries_on_error)
        .ADD_PROP_RW(BackendRequestParameters, retry_interval)
        .ADD_PROP_RW(BackendRequestParameters, retry_backoff_multiplier)
        ;

    bpy::class_<BackendInterface,
                boost::noncopyable>("BackendInterface",
                                    bpy::no_init)
        .def("list_objects",
             &list_objects,
             (bpy::args("backend_request_params") = BackendRequestParameters()),
             "list the objects on the backend\n"
             "@param backend_request_params, BackendRequestParameters\n"
             "@return a list of strings, the objects on the backend\n")
        .def("create_namespace",
             &create_namespace,
             (bpy::args("backend_request_params") = BackendRequestParameters()),
             "create a namespace on the backend\n"
             "@param backend_request_params, BackendRequestParameters\n")
        .def("remove_namespace",
             &BackendInterface::deleteNamespace,
             (bpy::args("backend_request_params") = BackendRequestParameters()),
             "Delete a namespace on the backend\n"
             "@param backend_request_params, BackendRequestParameters\n")
        .def("read",
             &BackendInterface::read,
             (bpy::args("path"),
              bpy::args("name"),
              bpy::args("insist_on_latest_version"),
              bpy::args("backend_request_params") = BackendRequestParameters()),
             "read and object from the backend into a destination\n"
             "@param a string, the path to download the object to\n"
             "@param a string, the object name\n"
             "@param an InsistOnLatestVersion, whether to insist on the latest version\n"
             "@param backend_request_params, BackendRequestParameters\n")
        .def("write",
             &write_object,
             (bpy::args("path"),
              bpy::args("name"),
              bpy::args("owerwrite") = OverwriteObject::F,
              bpy::args("backend_request_params") = BackendRequestParameters()),
             "write an object to the backend\n"
             "@param a string, the path to upload the object from\n"
             "@param a string, the object name\n"
             "@param a nOverwriteObject enum, whether to overwrite the object if it already exists\n"
             "@param backend_request_params, BackendRequestParameters\n")
        .def("object_exists",
             &BackendInterface::objectExists,
             (bpy::args("name"),
              bpy::args("backend_request_params") = BackendRequestParameters()),
             "Check whether and object exists on the backend.\n"
             "@param a string, the object name to check\n"
             "@param backend_request_params, BackendRequestParameters\n"
             "@returns a boolean, true if the object exists on the backend.")
        .def("namespace_exists",
             &BackendInterface::namespaceExists,
             (bpy::args("backend_request_params") = BackendRequestParameters()),
             "Check whether a namespace exists on the backend.\n"
             "@param backend_request_params, BackendRequestParameters\n"
             "@returns a boolean, true if the namespace exists on the backend.")
        .def("remove",
             &remove_object,
             (bpy::args("name"),
              bpy::args("object_may_not_exist"),
              bpy::args("backend_request_params") = BackendRequestParameters()),
             "Delete an object from the backend\n"
             "@param a string, the object name\n"
             "@param a bool, if true wont report an error if the object is not on the backend\n"
             "@param backend_request_params, BackendRequestParameters\n")
        .def("get_size",
             &BackendInterface::getSize,
             (bpy::args("name"),
              bpy::args("backend_request_params") = BackendRequestParameters()),
             "Get the size of an object on the backend\n"
             "@param a string, the object name in the namespace\n"
             "@param backend_request_params, BackendRequestParameters\n"
             "@return a number, size in bytes of the object on the backend")
        .def("__str__",
             &str)
    ;
}

}

}
