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

#include "ShmIdlInterface.h"
#include "ShmClient.h"

#include <youtils/Assert.h>
#include <youtils/UUID.h>
#include <youtils/OrbHelper.h>

namespace volumedriverfs
{

youtils::OrbHelper&
ShmClient::orb_helper()
{
    static youtils::OrbHelper orb_helper("ShmClient");
    return orb_helper;
}

ShmClient::ShmClient(const std::string& volume_name,
                     const std::string& vd_context_name,
                     const std::string& vd_context_kind,
                     const std::string& vd_object_name,
                     const std::string& vd_object_kind)
    : writerequest_msg_(new ShmWriteRequest())
    , writereply_msg_(new ShmWriteReply())
    , readrequest_msg_(new ShmReadRequest())
    , readreply_msg_(new ShmReadReply())
    , volume_name_(volume_name)
    , shm_segment_(new ipc::managed_shared_memory(ipc::open_only,
                                                  "openvstorage_segment"))
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vd_context_name,
                                                            vd_context_kind,
                                                            vd_object_name,
                                                            vd_object_kind);

    assert(not CORBA::is_nil(obj));
    volumefactory_ref_ = ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref_));

    ShmIdlInterface::CreateShmArguments createArguments;
    createArguments.volume_name = volume_name_.c_str();

    create_result.reset(volumefactory_ref_->create_shm_interface(createArguments));

    assert(youtils::UUID::isUUIDString(create_result->writerequest_uuid));
    assert(youtils::UUID::isUUIDString(create_result->writereply_uuid));
    assert(youtils::UUID::isUUIDString(create_result->readrequest_uuid));
    assert(youtils::UUID::isUUIDString(create_result->readreply_uuid));

    writerequest_mq_.reset(new ipc::message_queue(ipc::open_only,
                                                  create_result->writerequest_uuid));
    writereply_mq_.reset(new ipc::message_queue(ipc::open_only,
                                                create_result->writereply_uuid));
    readrequest_mq_.reset(new ipc::message_queue(ipc::open_only,
                                                 create_result->readrequest_uuid));
    readreply_mq_.reset(new ipc::message_queue(ipc::open_only,
                                               create_result->readreply_uuid));
    key_ = create_result->writerequest_uuid;
}

ShmClient::~ShmClient()
{
    try
    {
        volumefactory_ref_->stop_volume(volume_name_.c_str());
    }
    catch (CORBA::TRANSIENT& e)
    {
        //LOG_INFO("Transient exception when stopping mem client: server down?");
    }
    catch (std::exception& e)
    {
        //LOG_INFO("std::exception when stopping mem client: server down?");
        return;
    }
    catch (...)
    {
        //LOG_INFO("unknown exception when stopping mem client: server down?");
        return;
    }

    try
    {
        shm_segment_.reset();
    }
    catch (...)
    {
        return;
    }
}

void*
ShmClient::allocate(const uint64_t size_in_bytes)
{
    return shm_segment_->allocate(size_in_bytes,
                                  std::nothrow);
}

void
ShmClient::deallocate(void *shptr)
{
    shm_segment_->deallocate(shptr);
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
    writerequest_.handle = shm_segment_->get_handle_from_address(buf);
    writerequest_.opaque = reinterpret_cast<long>(opaque);

    try
    {
        writerequest_mq_->send(&writerequest_,
                               writerequest_size,
                               0);
    }
    catch (ipc::interprocess_exception& e)
    {
        errno = EIO;
        return -1;
    }
    return 0;
}

ssize_t
ShmClient::receive_write_reply(void **opaque)
{
    ShmWriteReply writereply_;
    unsigned int priority;
    ipc::message_queue::size_type received_size;

    try
    {
        writereply_mq_->receive(&writereply_,
                                writereply_size,
                                received_size,
                                priority);
        assert(received_size == writereply_size);
        *opaque = reinterpret_cast<void*>(writereply_.opaque);
    }
    catch (ipc::interprocess_exception& e)
    {
        errno = EIO;
        return -1;
    }
    return writereply_.size_in_bytes;
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
    readrequest_.handle = shm_segment_->get_handle_from_address(buf);
    readrequest_.opaque = reinterpret_cast<long>(opaque);

    try
    {
        readrequest_mq_->send(&readrequest_,
                              readrequest_size,
                              0);
    }
    catch (ipc::interprocess_exception& e)
    {
        errno = EIO;
        return -1;
    }
    return 0;
}

ssize_t
ShmClient::receive_read_reply(void **opaque)
{
    ShmReadReply readreply_;
    unsigned int priority;
    ipc::message_queue::size_type received_size;

    try
    {
        readreply_mq_->receive(&readreply_,
                               readreply_size,
                               received_size,
                               priority);
        assert(received_size == readreply_size);
        *opaque = reinterpret_cast<void*>(readreply_.opaque);
    }
    catch (ipc::interprocess_exception& e)
    {
        errno = EIO;
        return -1;
    }
    return readreply_.size_in_bytes;
}

bool
ShmClient::stop_reply_queues(int n)
{
    ShmReadReply readreply_;
    readreply_.opaque = 0;

    for (int i = 0; i < n; i++)
    {
        readreply_mq_->send(&readreply_,
                            readreply_size,
                            0);
    }

    ShmWriteReply writereply_;
    writereply_.opaque = 0;

    for (int i = 0; i < n; i++)
    {
        writereply_mq_->send(&writereply_,
                             writereply_size,
                             0);
    }
    return true;
}

void
ShmClient::create_volume(const std::string& volume_name,
                         const uint64_t volume_size)
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vd_context_name,
                                                            vd_context_kind,
                                                            vd_object_name,
                                                            vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volumefactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref));
    volumefactory_ref->create_volume(volume_name.c_str(),
                                     volume_size);
}

int
ShmClient::stat(struct stat *st)
{
    TODO("cnanakos: stat call from fs");
    st->st_blksize = 512;
    st->st_size = volume_size_in_bytes();
    return 0;
}
} //namespace

int
shm_create_handle(const char *volume_name,
                  ShmClientHandle* handle)
{
    try
    {
        *handle = new volumedriverfs::ShmClient(volume_name);
        return 0;
    }
    catch (ShmIdlInterface::VolumeDoesNotExist)
    {
        return -EACCES;
    }
    catch (std::exception& e)
    {
        return -EIO;
    }
    catch (...)
    {
        return -EIO;
    }
}

int
shm_destroy_handle(ShmClientHandle h)
{
    try
    {
        delete static_cast<volumedriverfs::ShmClient*>(h);
        return 0;
    }
    catch (std::exception& e)
    {
        return -1;
    }
    catch (...)
    {
        return -1;
    }
}

void*
shm_allocate(ShmClientHandle h,
             const uint64_t size_in_bytes)
{
    return static_cast<volumedriverfs::ShmClient*>(h)->allocate(size_in_bytes);
}

void
shm_deallocate(ShmClientHandle h,
               void *shptr)
{
    static_cast<volumedriverfs::ShmClient*>(h)->deallocate(shptr);
}

int
shm_send_read_request(ShmClientHandle h,
                      const void *buf,
                      const uint64_t size_in_bytes,
                      const uint64_t offset_in_bytes,
                      const void *opaque)
{
    return static_cast<volumedriverfs::ShmClient*>(h)->send_read_request(buf,
                                                                         size_in_bytes,
                                                                         offset_in_bytes,
                                                                         opaque);
}

ssize_t
shm_receive_read_reply(ShmClientHandle h,
                       void **opaque)
{
    return static_cast<volumedriverfs::ShmClient*>(h)->receive_read_reply(opaque);
}

int
shm_send_write_request(ShmClientHandle h,
                       const void *buf,
                       const uint64_t size_in_bytes,
                       const uint64_t offset_in_bytes,
                       const void *opaque)
{
    return static_cast<volumedriverfs::ShmClient*>(h)->send_write_request(buf,
                                                                          size_in_bytes,
                                                                          offset_in_bytes,
                                                                          opaque);
}

ssize_t
shm_receive_write_reply(ShmClientHandle h,
                        void **opaque)
{
    return static_cast<volumedriverfs::ShmClient*>(h)->receive_write_reply(opaque);
}

bool
shm_stop_reply_queues(ShmClientHandle h,
                      int n)
{
    return static_cast<volumedriverfs::ShmClient*>(h)->stop_reply_queues(n);
}

int
shm_stat(ShmClientHandle h,
         struct stat *st)
{
    assert(h);
    try
    {
        return static_cast<volumedriverfs::ShmClient*>(h)->stat(st);
    }
    catch (std::exception& e)
    {
        return -1;
    }
    catch (...)
    {
        return -1;
    }
}

int
shm_create_volume(const char* volume_name,
                  const uint64_t volume_size_in_bytes)
{
    try
    {
        volumedriverfs::ShmClient::create_volume(volume_name,
                                                 volume_size_in_bytes);
    }
    catch (ShmIdlInterface::VolumeExists)
    {
        return -EEXIST;
    }
    catch (std::exception& e)
    {
        return -EIO;
    }
    catch (...)
    {
        return -EIO;
    }
    return 0;
}

const char*
shm_get_key(ShmClientHandle h)
{
    return static_cast<volumedriverfs::ShmClient*>(h)->get_key().c_str();
}
