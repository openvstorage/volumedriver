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

namespace pythonbackend
{

void translate(const backend::BackendClientException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendBackendException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendFatalException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendStoreException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendRestoreException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendAssertionFailedException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendObjectDoesNotExistException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendObjectExistsException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendInputException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendCouldNotCreateNamespaceException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendCouldNotDeleteNamespaceException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendNamespaceAlreadyExistsException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendNamespaceDoesNotExistException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendConnectFailureException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendConnectionTimeoutException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendStreamClosedException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

void translate(const backend::BackendNotImplementedException& e)
{
    PyErr_SetString(PyExc_RuntimeError, e.what());
}

}

// Local Variables: **
// mode: c++ **
// End: **
