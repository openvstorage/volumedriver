// Copyright 2015 iNuron NV
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

#ifndef CONNECTION_ERRORS_H
#define CONNECTION_ERRORS_H

#include <exception>

#include <boost/python/def.hpp>
#include <boost/python/exception_translator.hpp>
#include <boost/python/module.hpp>

#include <backend/BackendException.h>

namespace pythonbackend
{

void translate(const backend::BackendClientException&);
void translate(const backend::BackendBackendException&);
void translate(const backend::BackendFatalException&);
void translate(const backend::BackendStoreException&);
void translate(const backend::BackendRestoreException&);
void translate(const backend::BackendOverwriteNotAllowedException&);
void translate(const backend::BackendObjectDoesNotExistException&);
void translate(const backend::BackendObjectExistsException&);
void translate(const backend::BackendInputException&);
void translate(const backend::BackendCouldNotCreateNamespaceException&);
void translate(const backend::BackendCouldNotDeleteNamespaceException&);
void translate(const backend::BackendNamespaceAlreadyExistsException&);
void translate(const backend::BackendNamespaceDoesNotExistException&);
void translate(const backend::BackendConnectFailureException&);
void translate(const backend::BackendConnectionTimeoutException&);
void translate(const backend::BackendStreamClosedException&);
void translate(const backend::BackendNotImplementedException&);

}



#endif // CONNECTION_ERRORS_H
