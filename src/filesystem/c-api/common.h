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

#ifndef __LIB_OVS_COMMON_H
#define __LIB_OVS_COMMON_H

#define ATTRIBUTE_UNUSED __attribute__((unused))

#include "volumedriver.h"

#include <string>
#include <libxio.h>

enum class RequestOp
{
    Noop,
    Read,
    Write,
    Flush,
    AsyncFlush,
    Open,
    Close,
};

enum class TransportType
{
    Error,
    SharedMemory,
    TCP,
    RDMA,
};

struct ovs_context_attr_t
{
    ovs_context_attr_t()
    : transport(TransportType::Error)
    , port(0)
    , network_qdepth(64)
    , enable_ha(false)
    {}

    TransportType transport;
    std::string host;
    int port;
    uint64_t network_qdepth;
    bool enable_ha;
};

struct ovs_buffer
{
    void *buf;
    size_t size;
    xio_reg_mem mem;
    bool from_mpool;
};

struct ovs_completion
{
    ovs_completion(ovs_callback_t cb, void *arg)
    : complete_cb(cb)
    , cb_arg(arg)
    , _on_wait(false)
    , _signaled(false)
    , _failed(false)
    , _rv(-1)
    {
        pthread_cond_init(&_cond, NULL);
        pthread_mutex_init(&_mutex, NULL);
    }

    ~ovs_completion()
    {
        pthread_mutex_destroy(&_mutex);
        pthread_cond_destroy(&_cond);
    }

    ovs_callback_t complete_cb;
    void *cb_arg;
    bool _on_wait;
    bool _signaled;
    bool _failed;
    ssize_t _rv;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
};

#endif
