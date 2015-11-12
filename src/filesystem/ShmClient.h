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

#ifndef __SHM_CLIENT_H_
#define __SHM_CLIENT_H_

#ifdef __cplusplus
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/errors.hpp>
#include <youtils/Logging.h>
#include <youtils/OrbHelper.h>

#include "ShmProtocol.h"
#include "ShmIdlInterface.h"

struct _ShmClientStruct
{};

namespace volumedriverfs
{
namespace ipc = boost::interprocess;
//class CreateResult;

class ShmClient final : public _ShmClientStruct
{
public:
    ShmClient(const std::string& volume_name,
              const std::string& context_name = vd_context_name,
              const std::string& context_kind = vd_context_kind,
              const std::string& object_name = vd_object_name,
              const std::string& object_kind = vd_object_kind);

    ~ShmClient();

    void*
    allocate(const uint64_t size_in_bytes);

    void
    deallocate(void* shptr);

    int
    send_read_request(const void *buf,
                      const uint64_t size_in_bytes,
                      const uint64_t offset_in_bytes,
                      const void *opaque);

    ssize_t
    receive_read_reply(void **opaque);

    int send_write_request(const void* buf,
                           const uint64_t size_in_bytes,
                           const uint64_t offset_in_bytes,
                           const void *opaque);

    ssize_t
    receive_write_reply(void **opaque);

    bool
    stop_reply_queues(int n);

    ssize_t
    send_write(const void* buf,
               const uint64_t size_in_bytes,
               const uint64_t offset_in_bytes);

    ssize_t
    send_read(void *buf,
              const uint64_t size_in_bytes,
              const uint64_t offset_in_bytes);

    int
    stat(struct stat *st);

    uint64_t
    volume_size_in_bytes() const
    {
        return create_result->volume_size_in_bytes;
    }

    const std::string&
    get_key() const
    {
        return key_;
    }

    void
    flush();

    static void
    create_volume(const std::string& volume_name,
                  const uint64_t volume_size);

private:

    static youtils::OrbHelper&
    orb_helper();

    std::unique_ptr<ipc::message_queue> writerequest_mq_;
    std::unique_ptr<ShmWriteRequest> writerequest_msg_;

    std::unique_ptr<ipc::message_queue> writereply_mq_;
    std::unique_ptr<ShmWriteReply> writereply_msg_;

    std::unique_ptr<ipc::message_queue> readrequest_mq_;
    std::unique_ptr<ShmReadRequest> readrequest_msg_;

    std::unique_ptr<ipc::message_queue> readreply_mq_;
    std::unique_ptr<ShmReadReply> readreply_msg_;

    ShmIdlInterface::VolumeFactory_var volumefactory_ref_;

    const std::string volume_name_;
    std::string key_;

    std::unique_ptr<ShmIdlInterface::CreateResult> create_result;

    std::unique_ptr<ipc::managed_shared_memory> shm_segment_;
};

} //namespace
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <stdint.h>

struct _ShmClientStruct;

typedef struct _ShmClientStruct* ShmClientHandle;


int
shm_create_handle(const char *volume_name,
                  ShmClientHandle* handle);

int
shm_destroy_handle(ShmClientHandle h);

void*
shm_allocate(ShmClientHandle h,
             const uint64_t size_in_bytes);

void
shm_deallocate(ShmClientHandle h,
               void *shmptr);

int
shm_send_read_request(ShmClientHandle h,
                      const void *buf,
                      const uint64_t size_in_bytes,
                      const uint64_t offset_in_bytes,
                      const void *opaque);

ssize_t
shm_receive_read_reply(ShmClientHandle h,
                       void **opaque);

int
shm_send_write_request(ShmClientHandle h,
                       const void *buf,
                       const uint64_t size_in_bytes,
                       const uint64_t offset_in_bytes,
                       const void *opaque);

ssize_t
shm_receive_write_reply(ShmClientHandle h,
                        void **opaque);

bool
shm_stop_reply_queues(ShmClientHandle h,
                      int n);

bool
shm_flush(ShmClientHandle h);

int
shm_stat(ShmClientHandle h,
         struct stat *st);

int
shm_create_volume(const char *volume_name,
                  const uint64_t volume_size_in_bytes);

const char*
shm_get_key(ShmClientHandle h);

#ifdef __cplusplus
}
#endif

#endif
