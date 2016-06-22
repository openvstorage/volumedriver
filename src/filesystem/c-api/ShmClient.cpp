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

#include "ShmClient.h"
#include "ShmOrbClient.h"

#include "../ShmProtocol.h"

#include <youtils/Assert.h>

namespace volumedriverfs
{

namespace ipc = boost::interprocess;

ShmClient::ShmClient(const ShmSegmentDetails& segment_details,
                     const std::string& vname,
                     const ShmIdlInterface::CreateResult& create_result)
    : writerequest_mq_(ipc::open_only,
                       create_result.writerequest_uuid)
    , writereply_mq_(ipc::open_only,
                     create_result.writereply_uuid)
    , readrequest_mq_(ipc::open_only,
                      create_result.readrequest_uuid)
    , readreply_mq_(ipc::open_only,
                    create_result.readreply_uuid)
    , shm_segment_(ipc::open_only,
                   segment_details.id().c_str())
    , segment_details_(segment_details)
    , vname_(vname)
    , vsize_(create_result.volume_size_in_bytes)
    , key_(create_result.writerequest_uuid)
{}

ShmClient::~ShmClient()
{
    try
    {
        ShmOrbClient(segment_details_).stop_volume_(vname_);
    }
    catch (std::exception& e)
    {
        //LOG_INFO("std::exception when stopping mem client: server down?");
    }
    catch (...)
    {
        //LOG_INFO("unknown exception when stopping mem client: server down?");
    }
}

void*
ShmClient::allocate(const uint64_t size_in_bytes)
{
    return shm_segment_.allocate(size_in_bytes,
                                 std::nothrow);
}

void
ShmClient::deallocate(void *shptr)
{
    shm_segment_.deallocate(shptr);
}

void*
ShmClient::get_address_from_handle(ipc::managed_shared_memory::handle_t handle)
{
    return shm_segment_.get_address_from_handle(handle);
}

ipc::managed_shared_memory::handle_t
ShmClient::get_handle_from_address(void *shptr)
{
    return shm_segment_.get_handle_from_address(shptr);
}

int
ShmClient::send_write_request(const void *buf,
                              const uint64_t size_in_bytes,
                              const uint64_t offset_in_bytes,
                              const void *opaque)
{
    ShmWriteRequest writerequest_;
    writerequest_.size_in_bytes = size_in_bytes;
    writerequest_.offset_in_bytes = offset_in_bytes;
    writerequest_.handle = shm_segment_.get_handle_from_address(buf);
    writerequest_.opaque = reinterpret_cast<uintptr_t>(opaque);

    try
    {
        writerequest_mq_.send(&writerequest_,
                               writerequest_size,
                               0);
    }
    catch (...)
    {
        errno = EIO;
        return -1;
    }
    return 0;
}

int
ShmClient::timed_send_write_request(const void *buf,
                                    const uint64_t size_in_bytes,
                                    const uint64_t offset_in_bytes,
                                    const void *opaque,
                                    const struct timespec* timeout)
{
    ShmWriteRequest writerequest_;
    writerequest_.size_in_bytes = size_in_bytes;
    writerequest_.offset_in_bytes = offset_in_bytes;
    writerequest_.handle = shm_segment_.get_handle_from_address(buf);
    writerequest_.opaque = reinterpret_cast<uintptr_t>(opaque);
    boost::posix_time::time_duration delay(boost::posix_time::seconds(timeout->tv_sec));
    boost::posix_time::ptime ptimeout =
        boost::posix_time::ptime(boost::posix_time::second_clock::universal_time());
    ptimeout += delay;

    try
    {
        int ret = writerequest_mq_.timed_send(&writerequest_,
                                               writerequest_size,
                                               0,
                                               ptimeout);
        if (not ret)
        {
            errno = ETIMEDOUT;
            return -1;
        }
    }
    catch (...)
    {
        errno = EIO;
        return -1;
    }
    return 0;
}

bool
ShmClient::receive_write_reply(size_t& size_in_bytes,
                               void **opaque)
{
    ShmWriteReply writereply_;
    unsigned int priority;
    ipc::message_queue::size_type received_size;

    try
    {
        writereply_mq_.receive(&writereply_,
                                writereply_size,
                                received_size,
                                priority);
        assert(received_size == writereply_size);
        *opaque = reinterpret_cast<void*>(writereply_.opaque);
    }
    catch (...)
    {
        *opaque = NULL;
        errno = EIO;
        return true;
    }
    size_in_bytes = writereply_.size_in_bytes;
    return writereply_.failed;
}

bool
ShmClient::timed_receive_write_reply(size_t& size_in_bytes,
                                     void **opaque,
                                     const struct timespec* timeout)
{
    ShmWriteReply writereply_;
    unsigned int priority;
    ipc::message_queue::size_type received_size;
    boost::posix_time::time_duration delay(boost::posix_time::seconds(timeout->tv_sec));
    boost::posix_time::ptime ptimeout =
        boost::posix_time::ptime(boost::posix_time::second_clock::universal_time());
    ptimeout += delay;

    try
    {
        int ret = writereply_mq_.timed_receive(&writereply_,
                                                writereply_size,
                                                received_size,
                                                priority,
                                                ptimeout);
        if (ret)
        {
            assert(received_size == writereply_size);
            *opaque = reinterpret_cast<void*>(writereply_.opaque);
        }
        else
        {
            *opaque = NULL;
            errno = ETIMEDOUT;
            return true;
        }
    }
    catch (...)
    {
        *opaque = NULL;
        errno = EIO;
        return true;
    }
    size_in_bytes = writereply_.size_in_bytes;
    return writereply_.failed;
}

int
ShmClient::send_read_request(const void *buf,
                             const uint64_t size_in_bytes,
                             const uint64_t offset_in_bytes,
                             const void *opaque)
{
    ShmReadRequest readrequest_;
    readrequest_.size_in_bytes = size_in_bytes;
    readrequest_.offset_in_bytes = offset_in_bytes;
    readrequest_.handle = shm_segment_.get_handle_from_address(buf);
    readrequest_.opaque = reinterpret_cast<uintptr_t>(opaque);

    try
    {
        readrequest_mq_.send(&readrequest_,
                              readrequest_size,
                              0);
    }
    catch (...)
    {
        errno = EIO;
        return -1;
    }
    return 0;
}

int
ShmClient::timed_send_read_request(const void *buf,
                                   const uint64_t size_in_bytes,
                                   const uint64_t offset_in_bytes,
                                   const void *opaque,
                                   const struct timespec* timeout)
{
    ShmReadRequest readrequest_;
    readrequest_.size_in_bytes = size_in_bytes;
    readrequest_.offset_in_bytes = offset_in_bytes;
    readrequest_.handle = shm_segment_.get_handle_from_address(buf);
    readrequest_.opaque = reinterpret_cast<uintptr_t>(opaque);
    boost::posix_time::time_duration delay(boost::posix_time::seconds(timeout->tv_sec));
    boost::posix_time::ptime ptimeout =
        boost::posix_time::ptime(boost::posix_time::second_clock::universal_time());
    ptimeout += delay;

    try
    {
        int ret = readrequest_mq_.timed_send(&readrequest_,
                                              readrequest_size,
                                              0,
                                              ptimeout);
        if (not ret)
        {
            errno = ETIMEDOUT;
            return -1;
        }
    }
    catch (...)
    {
        errno = EIO;
        return -1;
    }
    return 0;
}

bool
ShmClient::receive_read_reply(size_t& size_in_bytes,
                              void **opaque)
{
    ShmReadReply readreply_;
    unsigned int priority;
    ipc::message_queue::size_type received_size;

    try
    {
        readreply_mq_.receive(&readreply_,
                               readreply_size,
                               received_size,
                               priority);
        assert(received_size == readreply_size);
        *opaque = reinterpret_cast<void*>(readreply_.opaque);
    }
    catch (...)
    {
        *opaque = NULL;
        errno = EIO;
        return true;
    }
    size_in_bytes = readreply_.size_in_bytes;
    return readreply_.failed;
}

bool
ShmClient::timed_receive_read_reply(size_t& size_in_bytes,
                                    void **opaque,
                                    const struct timespec* timeout)
{
    ShmReadReply readreply_;
    unsigned int priority;
    ipc::message_queue::size_type received_size;
    boost::posix_time::time_duration delay(boost::posix_time::seconds(timeout->tv_sec));
    boost::posix_time::ptime ptimeout =
        boost::posix_time::ptime(boost::posix_time::second_clock::universal_time());
    ptimeout += delay;

    try
    {
        int ret = readreply_mq_.timed_receive(&readreply_,
                                               readreply_size,
                                               received_size,
                                               priority,
                                               ptimeout);
        if (ret)
        {
            assert(received_size == readreply_size);
            *opaque = reinterpret_cast<void*>(readreply_.opaque);
        }
        else
        {
            *opaque = NULL;
            errno = ETIMEDOUT;
            return true;
        }
    }
    catch (...)
    {
        *opaque = NULL;
        errno = EIO;
        return true;
    }
    size_in_bytes = readreply_.size_in_bytes;
    return readreply_.failed;
}

void
ShmClient::stop_reply_queues(int n)
{
    ShmReadReply readreply_;
    readreply_.opaque = 0;
    readreply_.stop = true;

    for (int i = 0; i < n; i++)
    {
        try
        {
            readreply_mq_.send(&readreply_,
                                readreply_size,
                                0);
        }
        catch (...)
        {}
    }

    ShmWriteReply writereply_;
    writereply_.opaque = 0;
    writereply_.stop = true;

    for (int i = 0; i < n; i++)
    {
        try
        {
            writereply_mq_.send(&writereply_,
                                 writereply_size,
                                 0);
        }
        catch (...)
        {}
    }
}

void
ShmClient::truncate(const uint64_t volume_size)
{
    ShmOrbClient(segment_details_).truncate_volume(vname_,
                                                   volume_size);
}

void
ShmClient::stat(struct stat& st)
{
    ShmOrbClient(segment_details_).stat(vname_,
                                        st);
}

} //namespace volumedriverfs
