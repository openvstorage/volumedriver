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

#include "../ShmSegmentDetails.h"

#include <boost/optional.hpp>

#define ATTRIBUTE_UNUSED __attribute__((unused))

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
    TransportType transport;
    std::string host;
    int port;
    uint64_t network_qdepth;
    boost::optional<ShmSegmentDetails> shm_segment_details;
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
    ovs_callback_t complete_cb;
    void *cb_arg;
    bool _on_wait;
    bool _calling;
    bool _signaled;
    bool _failed;
    ssize_t _rv;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
};
#endif
