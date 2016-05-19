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
