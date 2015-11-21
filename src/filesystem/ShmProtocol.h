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

#ifndef __SHM_PROTOCOL_H_
#define __SHM_PROTOCOL_H_

#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdint>

namespace volumedriverfs
{

const std::string vd_context_name("volumedriver");
const std::string vd_context_kind("storage");
const std::string vd_object_name("volumedriver_shm_interface");
const std::string vd_object_kind("storage");

const uint64_t max_write_queue_size = 128;
const uint64_t max_read_queue_size = 128;
const uint64_t max_reply_queue_size = 128;

struct ShmWriteRequest
{
    bool stop = false;
    uint64_t offset_in_bytes = 0;
    uint64_t size_in_bytes = 0;
    uintptr_t opaque;
    boost::interprocess::managed_shared_memory::handle_t handle;
};

static const uint64_t writerequest_size = sizeof(ShmWriteRequest);

struct ShmReadRequest
{
    bool stop = false;
    uint64_t offset_in_bytes = 0;
    uint64_t size_in_bytes = 0;
    uintptr_t opaque;
    boost::interprocess::managed_shared_memory::handle_t handle;
};

static const uint64_t readrequest_size = sizeof(ShmReadRequest);

struct ShmReadReply
{
    bool stop = false;
    uintptr_t opaque;
    uint64_t size_in_bytes = 0;
};

static const uint64_t readreply_size = sizeof(ShmReadReply);

struct ShmWriteReply
{
    bool stop = false;
    uintptr_t opaque;
    uint64_t size_in_bytes = 0;
};

static const uint64_t writereply_size = sizeof(ShmWriteReply);

}

#endif // __SHM_MSG_PROTOCOL_H_
