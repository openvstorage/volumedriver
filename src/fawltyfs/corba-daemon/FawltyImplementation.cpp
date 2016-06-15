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

#include "FawltyImplementation.h"
#include "CorbaFileSystem.h"
char* FawltyImplementation::ping(const char* mesg)
{
    return CORBA::string_dup(mesg);
}

void
FawltyImplementation::shutdown(bool /*wait_for_completion*/)
{
    stop_ = true;
}


Fawlty::FileSystem_ptr
FawltyImplementation::create(const char* backend_path,
                             const char* frontend_path,
                             const char* filesystem_name)
{

    POA_Fawlty::FileSystem_tie<CorbaFileSystem>* filesystem =
        new POA_Fawlty::FileSystem_tie<CorbaFileSystem>(new CorbaFileSystem(backend_path,
                                                                            frontend_path,
                                                                            filesystem_name),
                                                        poa_,
                                                        true);
    poa_->activate_object(filesystem);
    return filesystem->_this();
}

void
FawltyImplementation::release(Fawlty::FileSystem_ptr /*p*/)
{
    // CORBA::release(p);
}
