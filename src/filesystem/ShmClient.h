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

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/errors.hpp>
#include <youtils/Logging.h>
#include <youtils/OrbHelper.h>

#include "ShmProtocol.h"
#include "ShmIdlInterface.h"

namespace volumedriverfs
{
namespace ipc = boost::interprocess;

class ShmClient
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

    int
    timed_send_read_request(const void *buf,
                            const uint64_t size_in_bytes,
                            const uint64_t offset_in_bytes,
                            const void *opaque,
                            const struct timespec* timeout);

    bool
    receive_read_reply(size_t& size_in_bytes,
                       void **opaque);

    bool
    timed_receive_read_reply(size_t& size_in_bytes,
                             void **opaque,
                             const struct timespec* timeout);

    int send_write_request(const void* buf,
                           const uint64_t size_in_bytes,
                           const uint64_t offset_in_bytes,
                           const void *opaque);

    int timed_send_write_request(const void* buf,
                                 const uint64_t size_in_bytes,
                                 const uint64_t offset_in_bytes,
                                 const void *opaque,
                                 const struct timespec* timeout);

    bool
    receive_write_reply(size_t& size_in_bytes,
                        void **opaque);

    bool
    timed_receive_write_reply(size_t& size_in_bytes,
                              void **opaque,
                              const struct timespec* timeout);

    void
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

    void*
    get_address_from_handle(ipc::managed_shared_memory::handle_t handle);

    ipc::managed_shared_memory::handle_t
    get_handle_from_address(void *buf);

    static void
    init();

    static void
    fini();

private:
    static youtils::OrbHelper&
    orb_helper();

    std::unique_ptr<ipc::message_queue> writerequest_mq_;
    std::unique_ptr<ipc::message_queue> writereply_mq_;
    std::unique_ptr<ipc::message_queue> readrequest_mq_;
    std::unique_ptr<ipc::message_queue> readreply_mq_;

    ShmIdlInterface::VolumeFactory_var volumefactory_ref_;

    const std::string volume_name_;
    std::string key_;

    std::unique_ptr<ShmIdlInterface::CreateResult> create_result;

    std::unique_ptr<ipc::managed_shared_memory> shm_segment_;
};

typedef std::shared_ptr<ShmClient> ShmClientPtr;

} //namespace volumedriverfs
#endif
