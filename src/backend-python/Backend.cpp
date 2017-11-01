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

#include "ConnectionErrors.h"
#include "ConnectionInterface.h"
#include "ConnectionManager.h"

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/enum.hpp>
#include <boost/python/module.hpp>

#include <youtils/Gcrypt.h>
#include <youtils/Logger.h>

#include <youtils/python/BuildInfoAdapter.h>
#include <youtils/python/LoggingAdapter.h>

#include <backend/AlbaConfig.h>
#include <backend/BackendConfig.h>
#include <backend/LocalConfig.h>
#include <backend/MultiConfig.h>
#include <backend/S3Config.h>

#define MAKE_PYTHON_VD_BOOLEAN_ENUM(name, doc)     \
    enum_<name>(#name, doc)                     \
    .value("F", name::F)                        \
    .value("T", name::T);

namespace ypy = youtils::python;
namespace yt = youtils;

BOOST_PYTHON_MODULE(Backend)
{
    using namespace boost::python;
    using namespace pythonbackend;

#define EXN(ex)                                                         \
    {                                                                   \
        void (*tmp)(const ex&) = translate;                             \
        register_exception_translator<ex>(tmp); \
    }

    EXN(backend::BackendClientException);
    EXN(backend::BackendBackendException);
    EXN(backend::BackendStoreException);
    EXN(backend::BackendRestoreException);
    EXN(backend::BackendAssertionFailedException);
    EXN(backend::BackendRestoreException);
    EXN(backend::BackendAssertionFailedException);
    EXN(backend::BackendObjectDoesNotExistException);
    EXN(backend::BackendObjectExistsException);
    EXN(backend::BackendInputException);
    EXN(backend::BackendCouldNotCreateNamespaceException);
    EXN(backend::BackendCouldNotDeleteNamespaceException);
    EXN(backend::BackendNamespaceAlreadyExistsException);
    EXN(backend::BackendNamespaceDoesNotExistException);
    EXN(backend::BackendConnectFailureException);
    EXN(backend::BackendConnectionTimeoutException);
    EXN(backend::BackendStreamClosedException);
    EXN(backend::BackendNotImplementedException);

#undef EXN
    yt::Gcrypt::init_gcrypt();
    ypy::register_once<ypy::LoggingAdapter>();
    ypy::register_once<ypy::BuildInfoAdapter>();

    MAKE_PYTHON_VD_BOOLEAN_ENUM(OverwriteObject,
                                "Whether to overwrite an existing object in the backend, values are T and F");

    enum_<backend::BackendType>("BackendType",
                                "Type of backend connection.\n"
                                "Values are Local, S3 or Alba.")
        .value("Local", backend::BackendType::LOCAL)
        .value("S3", backend::BackendType::S3)
        .value("Alba", backend::BackendType::ALBA)       ;

    enum_<backend::S3Flavour>("S3Flavour",
                              "Flavour of the S3 backend.\n"
                              "Values are GCS, S3, SWIFT or WALRUS.")
        .value("GCS", backend::S3Flavour::GCS)
        .value("S3", backend::S3Flavour::S3)
        .value("SWIFT", backend::S3Flavour::SWIFT)
        .value("WALRUS", backend::S3Flavour::WALRUS)
        ;

    class_<ConnectionManager,
           boost::noncopyable>("ConnectionManager",
                               init<const boost::python::dict&>
                               ("Creates an interface to a backend described by the dict.\n"
                                "@param a dict, converted to a json tree to create a backend\n"))
        .def(init<const std::string&>("Creates a backend connection managers from a json file.\n"
                                      "@param a string, the path to a json configuration file"))
        .def("getConnection",
             &ConnectionManager::getConnection,
             return_value_policy<manage_new_object>(),
             "Creates an Connection to a backend")
        .def("__str__",  &ConnectionManager::str)
        .def("__repr__", &ConnectionManager::repr)
        ;

    class_<ConnectionInterface,
           boost::noncopyable>("ConnectionInterface",
                               no_init)
        .def("listNamespaces",
             &ConnectionInterface::listNamespaces,
             "Lists the namespaces on a backend\n"
             "@return a list of strings, the objects on the namespace")
        .def("listObjects",
             &ConnectionInterface::listObjects,
             "list the objects on the backend in a particular namespaces\n"
             "@param a string, the namespace to list\n"
             "@return a list of strings, the objects on the backend\n")
        .def("createNamespace",
             &ConnectionInterface::createNamespace,
             "create a namespace on the backend\n"
             "@param a string, the namespace to create\n")
        .def("deleteNamespace",
             &ConnectionInterface::deleteNamespace,
             "Delete a namespace on the backend\n"
             "@param a string, the namespace to delete")
        .def("read",
             &ConnectionInterface::read,
             "read and object from the backend into a destination\n"
             "@param a string, the namespace to read from\n"
             "@param a string, the filename to write the object to\n"
             "@param a string, the object name\n"
             "@param a boolean, whether to insist on the latest version\n")
        .def("write",
             &ConnectionInterface::write,
             "write an object to the backend\n"
             "@param a string, the namespace to write to\n"
             "@param a string, the filename to write\n"
             "@param a string, the object name in the namesxspace\n"
             "@param a OverwriteObject enum, whether to overwrite the object if it already exists.")
        .def("objectExists",
             &ConnectionInterface::objectExists,
             "Check whether and object exists on the backend.\n"
             "@param a string, the namespace to check\n"
             "@param a string, the object name to check\n"
             "@returns a boolean, true if the object exists on the backend.")
        .def("namespaceExists",
             &ConnectionInterface::namespaceExists,
             "Check whether a namespace exists on the backend.\n"
             "@param a string, the namespace to check\n"
             "@returns a boolean, true if the namespace exists on the backend.")
        .def("remove",
             &ConnectionInterface::remove,
             "Delete an object from the backend\n"
             "@param a string, the namespace to delete the object from\n"
             "@param a string, the object name in the namespace\n"
             "@param a bool, if true wont report an error if the object is not on the backend")
        .def("getSize",
             &ConnectionInterface::getSize,
             "Get the size of an object on the backend\n"
             "@param a string, the namespace where the object resides\n"
             "@param a string, the object name in the namespace\n"
             "@return a number, size in bytes of the object on the backend")
        .def("__str__",
             &ConnectionInterface::str)
        .def("__repr__",
             &ConnectionInterface::str);
}

// Local Variables: **
// mode: c++ **
// End: **
