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

#ifndef BACKEND_EXCEPTION_H_
#define BACKEND_EXCEPTION_H_

#include <youtils/IOException.h>

namespace backend
{
MAKE_EXCEPTION(BackendException, fungi::IOException);


// Y42 : NEEDS TO HAVE CONSTRUCTORS THAT ALLOW YOU TO PASS A NON GENERIC DESCRIPTION
// DONT OVERRIDE THE WHAT WITHOUT USING THE fungi::IOException WHAT...
// ALL THIS STUFF IS WAY TOO MUCH TIED TO THE DSS BACKEND.
#define DEFINE_BACKEND_EXCEPTION(errclass, desc)        \
    class Backend##errclass##Exception                   \
        : public BackendException                        \
    {                                                    \
        public:                                          \
        Backend##errclass##Exception()                   \
            : BackendException("")                       \
        {}                                               \
                                                         \
        const char*                                      \
        what() const throw()                             \
        {                                                \
            return "BackendException: " desc;            \
        }                                                \
    }

// add error classes as needed:
// client error that violates the backend's expectations
DEFINE_BACKEND_EXCEPTION(Client, "client side error");

// backend internal error - failure to connect or timeouts
DEFINE_BACKEND_EXCEPTION(Backend, "backend internal error");

// the backend's means to tell us to give up (?)
DEFINE_BACKEND_EXCEPTION(Fatal, "fatal backend error");

// backend had a problem storing the object
DEFINE_BACKEND_EXCEPTION(Store, "store error");

// backend had a problem restoring the object. to be investigated
// whether this includes success retrieving it from the network / $wherever but
// failure to write to a local file. the latter should be mapped to an output
// error instead.
DEFINE_BACKEND_EXCEPTION(Restore, "restore error");

// attempt to overwrite but not explicity requested
DEFINE_BACKEND_EXCEPTION(OverwriteNotAllowed, "overwrite not allowed");

// self explanatory
DEFINE_BACKEND_EXCEPTION(ObjectDoesNotExist, "object does not exist");

DEFINE_BACKEND_EXCEPTION(ObjectExists, "object exists");

// problem with the input file, e.g. a crc error
DEFINE_BACKEND_EXCEPTION(Input, "input error");

// problem with the output file
DEFINE_BACKEND_EXCEPTION(Output, "output error");

DEFINE_BACKEND_EXCEPTION(CouldNotCreateNamespace, "Could not create namespace");

DEFINE_BACKEND_EXCEPTION(CouldNotDeleteNamespace, "Could not delete namespace");

DEFINE_BACKEND_EXCEPTION(CouldNotClearNamespace, "Could not clear namespace");
// self-explanatory
DEFINE_BACKEND_EXCEPTION(NamespaceAlreadyExists, "namespace already exists");

// self-explanatory
DEFINE_BACKEND_EXCEPTION(NamespaceDoesNotExist, "namespace does not exist");

// self-explanatory
DEFINE_BACKEND_EXCEPTION(ConnectFailure, "failed to connect");

// self-explanatory
DEFINE_BACKEND_EXCEPTION(ConnectionTimeout, "connection timed out");

// api misuse: trying to operate on a source / sink that's been closed already.
DEFINE_BACKEND_EXCEPTION(StreamClosed, "stream already closed");

DEFINE_BACKEND_EXCEPTION(NotImplemented,
                          "this functionality is not present on this backend");

DEFINE_BACKEND_EXCEPTION(UnAuthorized,
                         "Could not authorize the request for this user");

DEFINE_BACKEND_EXCEPTION(NoMultiBackendAvailable,
                         "No MULTI backend available");

DEFINE_BACKEND_EXCEPTION(UniqueObjectTagMismatch,
                         "UniqueObjectTag does not match");

//#undef DEFINE_BACKEND_EXCEPTION

}

#endif // !BACKEND_EXCEPTION_H_

// Local Variables: **
// mode: c++ **
// End: **
