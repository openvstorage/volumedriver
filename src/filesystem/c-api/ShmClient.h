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

#ifndef __SHM_CLIENT_H_
#define __SHM_CLIENT_H_

#include "../ShmProtocol.h"
#include "../ShmIdlInterface.h"
#include "internal.h"

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/errors.hpp>
#include <youtils/Logging.h>
#include <youtils/OrbHelper.h>

namespace libovsvolumedriver
{
namespace ipc = boost::interprocess;

class ShmClient
{
public:
    ShmClient(const std::string& volume_name,
              const std::string& context_name = volumedriverfs::vd_context_name,
              const std::string& context_kind = volumedriverfs::vd_context_kind,
              const std::string& object_name = volumedriverfs::vd_object_name,
              const std::string& object_kind = volumedriverfs::vd_object_kind);

    ~ShmClient();

    void*
    allocate(const uint64_t size_in_bytes);

    void
    deallocate(void* shptr);

    int
    send_read_request(const void *buf,
                      const uint64_t size_in_bytes,
                      const uint64_t offset_in_bytes,
                      const ovs_aio_request *request);

    int
    timed_send_read_request(const void *buf,
                            const uint64_t size_in_bytes,
                            const uint64_t offset_in_bytes,
                            const ovs_aio_request *request,
                            const struct timespec* timeout);

    bool
    receive_read_reply(size_t& size_in_bytes,
                       ovs_aio_request **request);

    bool
    timed_receive_read_reply(size_t& size_in_bytes,
                             ovs_aio_request **request,
                             const struct timespec* timeout);

    int send_write_request(const void* buf,
                           const uint64_t size_in_bytes,
                           const uint64_t offset_in_bytes,
                           const ovs_aio_request *request);

    int timed_send_write_request(const void* buf,
                                 const uint64_t size_in_bytes,
                                 const uint64_t offset_in_bytes,
                                 const ovs_aio_request *request,
                                 const struct timespec* timeout);

    bool
    receive_write_reply(size_t& size_in_bytes,
                        ovs_aio_request **request);

    bool
    timed_receive_write_reply(size_t& size_in_bytes,
                              ovs_aio_request **request,
                              const struct timespec* timeout);

    void
    stop_reply_queues(int n);

    int
    stat(const std::string& volume_name,
         struct stat *st);

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

    static void
    remove_volume(const std::string& volume_name);

    static void
    truncate_volume(const std::string& volume_name,
                    const uint64_t volume_size);

    static void
    create_snapshot(const std::string& volume_name,
                    const std::string& snapshot_name,
                    const int64_t timeout);

    static void
    rollback_snapshot(const std::string& volume_name,
                      const std::string& snapshot_name);

    static void
    delete_snapshot(const std::string& volume_name,
                    const std::string& snapshot_name);

    static std::vector<std::string>
    list_snapshots(const std::string& volume_name,
                   uint64_t *size);

    static int
    is_snapshot_synced(const std::string& volume_name,
                       const std::string& snapshot_name);

    static std::vector<std::string>
    list_volumes();

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

} //namespace libovsvolumedriver
#endif
