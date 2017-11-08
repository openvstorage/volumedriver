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

#include "Exceptions.h"

#include "../BackendException.h"

#include <boost/python/def.hpp>
#include <boost/python/exception_translator.hpp>

namespace backend
{

namespace python
{

namespace bpy = boost::python;

namespace
{

template<typename E>
void
translate(const E& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

}

DEFINE_PYTHON_WRAPPER(Exceptions)
{
#define EXN(E)                                                          \
    {                                                                   \
        void (*fun)(const E&) = translate<E>;                           \
        bpy::register_exception_translator<E>(fun);                     \
    }

    EXN(BackendAssertionFailedException);
    EXN(BackendClientException);
    EXN(BackendBackendException);
    EXN(BackendStoreException);
    EXN(BackendRestoreException);
    EXN(BackendAssertionFailedException);
    EXN(BackendRestoreException);
    EXN(BackendAssertionFailedException);
    EXN(BackendObjectDoesNotExistException);
    EXN(BackendObjectExistsException);
    EXN(BackendInputException);
    EXN(BackendCouldNotCreateNamespaceException);
    EXN(BackendCouldNotDeleteNamespaceException);
    EXN(BackendNamespaceAlreadyExistsException);
    EXN(BackendNamespaceDoesNotExistException);
    EXN(BackendConnectFailureException);
    EXN(BackendConnectionTimeoutException);
    EXN(BackendStreamClosedException);
    EXN(BackendNotImplementedException);
}

}

}
