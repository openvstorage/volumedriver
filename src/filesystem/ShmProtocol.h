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

#ifndef __SHM_PROTOCOL_H_
#define __SHM_PROTOCOL_H_

#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdint>

namespace volumedriverfs
{

// TODO: only declare these here (extern). Convert them into static members of a struct?
const std::string vd_context_kind("vrouter_cluster_id");
const std::string vd_object_kind("vrouter");

const uint64_t max_write_queue_size = 128;
const uint64_t max_read_queue_size = 128;
const uint64_t max_reply_queue_size = 128;

struct ShmWriteRequest
{
    bool stop = false;
    uint64_t offset_in_bytes = 0;
    size_t  size_in_bytes = 0;
    uintptr_t opaque;
    boost::interprocess::managed_shared_memory::handle_t handle;
};

// TODO: no static vars in a header.
static const uint64_t writerequest_size = sizeof(ShmWriteRequest);

struct ShmReadRequest
{
    bool stop = false;
    uint64_t offset_in_bytes = 0;
    size_t  size_in_bytes = 0;
    uintptr_t opaque;
    boost::interprocess::managed_shared_memory::handle_t handle;
};

static const uint64_t readrequest_size = sizeof(ShmReadRequest);

struct ShmReadReply
{
    bool stop = false;
    bool failed = false;
    uintptr_t opaque;
    size_t  size_in_bytes = 0;
};

static const uint64_t readreply_size = sizeof(ShmReadReply);

struct ShmWriteReply
{
    bool stop = false;
    bool failed = false;
    uintptr_t opaque;
    size_t size_in_bytes = 0;
};

static const uint64_t writereply_size = sizeof(ShmWriteReply);

}

#endif // __SHM_PROTOCOL_H_
