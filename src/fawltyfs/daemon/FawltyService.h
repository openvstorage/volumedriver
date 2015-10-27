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
