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


//#undef DEFINE_BACKEND_EXCEPTION

}

#endif // !BACKEND_EXCEPTION_H_

// Local Variables: **
// mode: c++ **
// End: **
