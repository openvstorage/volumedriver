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

#ifndef FAWLTY_SERVICE_H_
#define FAWLTY_SERVICE_H_
#include "Fawlty.h"
#include <thrift/server/TSimpleServer.h>
#include <youtils/Logging.h>
using namespace fawlty_daemon;

class FawltyService :
    virtual public fawlty_daemon::FawltyIf
{
    typedef apache::thrift::server::TSimpleServer ServerType;

    DECLARE_LOGGER("FawltyService");

public:
    FawltyService();

    virtual filesystem_handle_t
    make_file_system(const backend_path_t& backend_path,
                     const frontend_path_t& frontend_path,
                     const fuse_args_t& fuse_args,
                     const filesystem_name_t& filesystem_name);

    virtual void
    mount(const filesystem_handle_t filesystem_handle);


    virtual void
    umount(const filesystem_handle_t filesystem_handle);

    virtual int32_t
    ping();

    virtual void
    stop();

    void
    set_server(ServerType& server)
    {
        server_ = &server;
    }

    ServerType* server_;
};

#endif // FAWLTY_SERVICE_H_
