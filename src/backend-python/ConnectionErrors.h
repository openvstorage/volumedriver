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
