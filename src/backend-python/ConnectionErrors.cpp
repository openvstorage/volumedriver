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

void translate(const backend::BackendOverwriteNotAllowedException& e)
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
