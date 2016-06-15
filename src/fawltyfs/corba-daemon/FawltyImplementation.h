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

#ifndef FAWLTY_IMPLEMENTATION_H
#define FAWLTY_IMPLEMENTATION_H
#include "fawlty.hh"

class FawltyImplementation
{
public:
    FawltyImplementation(bool& stop_,
                         PortableServer::POA_ptr poa)
        : poa_(poa)
        , stop_(stop_)
    {}


    ~FawltyImplementation()
    {}

    char* ping(const char* string);

    void
    shutdown(bool wait_for_completion);

    Fawlty::FileSystem_ptr
    create(const char* backend_path,
           const char* frontend_path,
           const char* filesystem_name);

    void
    release(Fawlty::FileSystem_ptr p);

private:
    PortableServer::POA_ptr poa_;
    bool& stop_;
};

#endif
