// Copyright 2016 iNuron NV
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

#ifndef __NETWORK_XIO_REQUEST_H_
#define __NETWORK_XIO_REQUEST_H_

#include "NetworkXioWork.h"
#include "NetworkXioCommon.h"

namespace volumedriverfs
{

struct NetworkXioClientData;

struct NetworkXioRequest
{
    NetworkXioMsgOpcode op;

    void *data;
    unsigned int data_len;
    size_t size;
    uint64_t offset;

    ssize_t retval;
    int errval;
    uintptr_t opaque;

    Work work;

    xio_msg *xio_req;
    xio_msg xio_reply;
    xio_reg_mem reg_mem;
    bool from_pool;

    NetworkXioClientData *cd;

    std::string s_msg;
};

class NetworkXioServer;
class NetworkXioIOHandler;

struct NetworkXioClientData
{
    xio_session *session;
    xio_connection *conn;
    xio_mempool *mpool;
    std::atomic<bool> disconnected;
    std::atomic<uint64_t> refcnt;
    NetworkXioServer *server;
    NetworkXioIOHandler *ioh;
    std::list<NetworkXioRequest*> done_reqs;
};

} //namespace

#endif //__NETWORK_XIO_REQUEST_H_
