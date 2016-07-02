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

#include "ShmIdlInterface.h"
#include "ShmSegmentDetails.h"

#include "../ShmIdlInterface.h"
#include "../ShmProtocol.h"
#include "../ShmSegmentDetails.h"

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/errors.hpp>

namespace volumedriverfs
{

class ShmOrbClient;

class ShmClient
{
public:
    ~ShmClient();

    ShmClient(const ShmClient&) = delete;

    ShmClient&
    operator=(const ShmClient&) = delete;

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

    int
    stat(const std::string& volume_name,
         struct stat *st);

    const std::string&
    get_key() const
    {
        return key_;
    }

    const std::string&
    volume_name() const
    {
        return vname_;
    }

    void
    flush();

    void
    truncate(const uint64_t volume_size);

    void
    stat(struct stat&);

    void
    stop_reply_queues(int n);

    void*
    get_address_from_handle(boost::interprocess::managed_shared_memory::handle_t handle);

    boost::interprocess::managed_shared_memory::handle_t
    get_handle_from_address(void *buf);

private:
    friend class ShmOrbClient;

    ShmClient(const ShmSegmentDetails&,
              const std::string& vname,
              const ShmIdlInterface::CreateResult&);

    boost::interprocess::message_queue writerequest_mq_;
    boost::interprocess::message_queue writereply_mq_;
    boost::interprocess::message_queue readrequest_mq_;
    boost::interprocess::message_queue readreply_mq_;
    boost::interprocess::managed_shared_memory shm_segment_;

    const ShmSegmentDetails segment_details_;
    const std::string vname_;
    const std::string key_;
};

} //namespace volumedriverfs

#endif
