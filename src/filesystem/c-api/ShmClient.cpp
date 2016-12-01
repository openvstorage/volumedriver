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

#include "Logger.h"
#include "../ShmCommon.h"
#include "ShmClient.h"

#include <youtils/Assert.h>
#include <youtils/UUID.h>
#include <youtils/OrbHelper.h>

namespace libovsvolumedriver
{

namespace ipc = boost::interprocess;
namespace bpt = boost::posix_time;
namespace yt = youtils;
namespace vfs = volumedriverfs;

namespace
{

TODO("AR: get rid of static OrbHelper");
// This is just a stopgap measure to fix the problems with the ShmServerTests (race
// with remote server startup, static OrbHelper instance used across several tests).
// In the long run it's cleaner to have an OrbHelper instance per ovs_context_t
// (e.g. via the associated ShmClient) if possible?
// Do we need locking around OrbHelper calls?
DECLARE_LOGGER("ShmClientUtils");

boost::mutex orb_helper_lock;
std::unique_ptr<yt::OrbHelper> orb_helper_instance;

#define LOCK_ORB_HELPER()                       \
    boost::lock_guard<decltype(orb_helper_lock)> g(orb_helper_lock)

}

void
ShmClient::init()
{
    LOCK_ORB_HELPER();

    VERIFY(not orb_helper_instance);
    orb_helper_instance = std::make_unique<yt::OrbHelper>("ShmClient");
}

void
ShmClient::fini()
{
    LOCK_ORB_HELPER();

    VERIFY(orb_helper_instance);
    orb_helper_instance = nullptr;
}

youtils::OrbHelper&
ShmClient::orb_helper()
{
    LOCK_ORB_HELPER();

    if (not orb_helper_instance)
    {
        orb_helper_instance = std::make_unique<yt::OrbHelper>("ShmClient");
    }
    return *orb_helper_instance;
}

ShmClient::ShmClient(const std::string& volume_name,
                     const std::string& context_name,
                     const std::string& context_kind,
                     const std::string& object_name,
                     const std::string& object_kind)
    : volume_name_(volume_name)
    , shm_segment_(new ipc::managed_shared_memory(ipc::open_only,
                                                  ShmSegmentDetails::Name()))
{
    LIBLOGID_INFO("volume name: " << volume_name
                  << ",context name: " << context_name
                  << ",context kind: " << context_kind
                  << ", object name: " << object_name
                  << ", object kind: " << object_kind);
    CORBA::Object_var obj = orb_helper().getObjectReference(context_name,
                                                            context_kind,
                                                            object_name,
                                                            object_kind);

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
        LIBLOGID_INFO("Transient exception when stopping mem client: server down?");
    }
    catch (std::exception& e)
    {
        LIBLOGID_INFO("std::exception when stopping mem client: server down?");
    }
    catch (...)
    {
        LIBLOGID_INFO("unknown exception when stopping mem client: server down?");
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

void*
ShmClient::get_address_from_handle(ipc::managed_shared_memory::handle_t handle)
{
    return shm_segment_->get_address_from_handle(handle);
}

ipc::managed_shared_memory::handle_t
ShmClient::get_handle_from_address(void *shptr)
{
    return shm_segment_->get_handle_from_address(shptr);
}

int
ShmClient::send_write_request(const void *buf,
                              const uint64_t size_in_bytes,
                              const uint64_t offset_in_bytes,
                              const ovs_aio_request *request)
{
    vfs::ShmWriteRequest writerequest_;
    writerequest_.size_in_bytes = size_in_bytes;
    writerequest_.offset_in_bytes = offset_in_bytes;
    writerequest_.handle = shm_segment_->get_handle_from_address(buf);
    writerequest_.opaque = reinterpret_cast<uintptr_t>(request);

    try
    {
        writerequest_mq_->send(&writerequest_,
                               sizeof(vfs::ShmWriteRequest),
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
                                    const ovs_aio_request *request,
                                    const struct timespec* timeout)
{
    vfs::ShmWriteRequest writerequest_;
    writerequest_.size_in_bytes = size_in_bytes;
    writerequest_.offset_in_bytes = offset_in_bytes;
    writerequest_.handle = shm_segment_->get_handle_from_address(buf);
    writerequest_.opaque = reinterpret_cast<uintptr_t>(request);
    bpt::time_duration delay(bpt::seconds(timeout->tv_sec));
    bpt::ptime ptimeout = bpt::ptime(bpt::second_clock::universal_time());
    ptimeout += delay;

    try
    {
        int ret = writerequest_mq_->timed_send(&writerequest_,
                                               sizeof(vfs::ShmWriteRequest),
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
                               ovs_aio_request **request)
{
    vfs::ShmWriteReply writereply_;
    unsigned int priority;
    ipc::message_queue::size_type received_size;

    try
    {
        writereply_mq_->receive(&writereply_,
                                sizeof(vfs::ShmWriteReply),
                                received_size,
                                priority);
        assert(received_size == sizeof(vfs::ShmWriteReply));
        *request = reinterpret_cast<ovs_aio_request*>(writereply_.opaque);
    }
    catch (...)
    {
        *request = NULL;
        errno = EIO;
        return true;
    }
    size_in_bytes = writereply_.size_in_bytes;
    return writereply_.failed;
}

bool
ShmClient::timed_receive_write_reply(size_t& size_in_bytes,
                                     ovs_aio_request **request,
                                     const struct timespec* timeout)
{
    vfs::ShmWriteReply writereply_;
    unsigned int priority;
    ipc::message_queue::size_type received_size;
    bpt::time_duration delay(bpt::seconds(timeout->tv_sec));
    bpt::ptime ptimeout = bpt::ptime(bpt::second_clock::universal_time());
    ptimeout += delay;

    try
    {
        int ret = writereply_mq_->timed_receive(&writereply_,
                                                sizeof(vfs::ShmWriteReply),
                                                received_size,
                                                priority,
                                                ptimeout);
        if (ret)
        {
            assert(received_size == sizeof(vfs::ShmWriteReply));
            *request = reinterpret_cast<ovs_aio_request*>(writereply_.opaque);
        }
        else
        {
            *request = NULL;
            errno = ETIMEDOUT;
            return true;
        }
    }
    catch (...)
    {
        *request = NULL;
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
                             const ovs_aio_request *request)
{
    vfs::ShmReadRequest readrequest_;
    readrequest_.size_in_bytes = size_in_bytes;
    readrequest_.offset_in_bytes = offset_in_bytes;
    readrequest_.handle = shm_segment_->get_handle_from_address(buf);
    readrequest_.opaque = reinterpret_cast<uintptr_t>(request);

    try
    {
        readrequest_mq_->send(&readrequest_,
                              sizeof(vfs::ShmReadRequest),
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
                                   const ovs_aio_request *request,
                                   const struct timespec* timeout)
{
    vfs::ShmReadRequest readrequest_;
    readrequest_.size_in_bytes = size_in_bytes;
    readrequest_.offset_in_bytes = offset_in_bytes;
    readrequest_.handle = shm_segment_->get_handle_from_address(buf);
    readrequest_.opaque = reinterpret_cast<uintptr_t>(request);
    bpt::time_duration delay(bpt::seconds(timeout->tv_sec));
    bpt::ptime ptimeout = bpt::ptime(bpt::second_clock::universal_time());
    ptimeout += delay;

    try
    {
        int ret = readrequest_mq_->timed_send(&readrequest_,
                                              sizeof(vfs::ShmReadRequest),
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
                              ovs_aio_request **request)
{
    vfs::ShmReadReply readreply_;
    unsigned int priority;
    ipc::message_queue::size_type received_size;

    try
    {
        readreply_mq_->receive(&readreply_,
                               sizeof(vfs::ShmReadReply),
                               received_size,
                               priority);
        assert(received_size == sizeof(vfs::ShmReadReply));
        *request = reinterpret_cast<ovs_aio_request*>(readreply_.opaque);
    }
    catch (...)
    {
        *request = NULL;
        errno = EIO;
        return true;
    }
    size_in_bytes = readreply_.size_in_bytes;
    return readreply_.failed;
}

bool
ShmClient::timed_receive_read_reply(size_t& size_in_bytes,
                                    ovs_aio_request **request,
                                    const struct timespec* timeout)
{
    vfs::ShmReadReply readreply_;
    unsigned int priority;
    ipc::message_queue::size_type received_size;
    bpt::time_duration delay(bpt::seconds(timeout->tv_sec));
    bpt::ptime ptimeout = bpt::ptime(bpt::second_clock::universal_time());
    ptimeout += delay;

    try
    {
        int ret = readreply_mq_->timed_receive(&readreply_,
                                               sizeof(vfs::ShmReadReply),
                                               received_size,
                                               priority,
                                               ptimeout);
        if (ret)
        {
            assert(received_size == sizeof(vfs::ShmReadReply));
            *request = reinterpret_cast<ovs_aio_request*>(readreply_.opaque);
        }
        else
        {
            *request = NULL;
            errno = ETIMEDOUT;
            return true;
        }
    }
    catch (...)
    {
        *request = NULL;
        errno = EIO;
        return true;
    }
    size_in_bytes = readreply_.size_in_bytes;
    return readreply_.failed;
}

void
ShmClient::stop_reply_queues(int n)
{
    vfs::ShmReadReply readreply_;
    readreply_.opaque = 0;
    readreply_.stop = true;

    for (int i = 0; i < n; i++)
    {
        try
        {
            readreply_mq_->send(&readreply_,
                                sizeof(vfs::ShmReadReply),
                                0);
        }
        catch (...)
        {}
    }

    vfs::ShmWriteReply writereply_;
    writereply_.opaque = 0;
    writereply_.stop = true;

    for (int i = 0; i < n; i++)
    {
        try
        {
            writereply_mq_->send(&writereply_,
                                 sizeof(vfs::ShmWriteReply),
                                 0);
        }
        catch (...)
        {}
    }
}

void
ShmClient::create_volume(const std::string& volume_name,
                         const uint64_t volume_size)
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vfs::vd_context_name,
                                                            vfs::vd_context_kind,
                                                            vfs::vd_object_name,
                                                            vfs::vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volumefactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref));
    volumefactory_ref->create_volume(volume_name.c_str(),
                                     volume_size);
}

void
ShmClient::remove_volume(const std::string& volume_name)
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vfs::vd_context_name,
                                                            vfs::vd_context_kind,
                                                            vfs::vd_object_name,
                                                            vfs::vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volumefactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref));
    volumefactory_ref->remove_volume(volume_name.c_str());
}

void
ShmClient::truncate_volume(const std::string& volume_name,
                           const uint64_t volume_size)
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vfs::vd_context_name,
                                                            vfs::vd_context_kind,
                                                            vfs::vd_object_name,
                                                            vfs::vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volumefactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref));
    volumefactory_ref->truncate_volume(volume_name.c_str(),
                                       volume_size);
}

void
ShmClient::create_snapshot(const std::string& volume_name,
                           const std::string& snapshot_name,
                           const int64_t timeout)
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vfs::vd_context_name,
                                                            vfs::vd_context_kind,
                                                            vfs::vd_object_name,
                                                            vfs::vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volumefactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref));
    volumefactory_ref->create_snapshot(volume_name.c_str(),
                                       snapshot_name.c_str(),
                                       timeout);
}

void
ShmClient::rollback_snapshot(const std::string& volume_name,
                             const std::string& snapshot_name)
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vfs::vd_context_name,
                                                            vfs::vd_context_kind,
                                                            vfs::vd_object_name,
                                                            vfs::vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volumefactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref));
    volumefactory_ref->rollback_snapshot(volume_name.c_str(),
                                         snapshot_name.c_str());
}

void
ShmClient::delete_snapshot(const std::string& volume_name,
                           const std::string& snapshot_name)
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vfs::vd_context_name,
                                                            vfs::vd_context_kind,
                                                            vfs::vd_object_name,
                                                            vfs::vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volumefactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref));
    volumefactory_ref->delete_snapshot(volume_name.c_str(),
                                       snapshot_name.c_str());
}

std::vector<std::string>
ShmClient::list_snapshots(const std::string& volume_name,
                          uint64_t *size)
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vfs::vd_context_name,
                                                            vfs::vd_context_kind,
                                                            vfs::vd_object_name,
                                                            vfs::vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volumefactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref));

    ShmIdlInterface::StringSequence_var results;
    CORBA::ULongLong c_size;
    std::vector<std::string> snaps;
    volumefactory_ref->list_snapshots(volume_name.c_str(),
                                      results.out(),
                                      c_size);
    for (unsigned int i = 0; i < results->length(); i++)
    {
        snaps.push_back(results[i].in());
    }
    *size = c_size;
    return snaps;
}

int
ShmClient::is_snapshot_synced(const std::string& volume_name,
                              const std::string& snapshot_name)
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vfs::vd_context_name,
                                                            vfs::vd_context_kind,
                                                            vfs::vd_object_name,
                                                            vfs::vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volumefactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref));
    return volumefactory_ref->is_snapshot_synced(volume_name.c_str(),
                                                 snapshot_name.c_str());
}

std::vector<std::string>
ShmClient::list_volumes()
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vfs::vd_context_name,
                                                            vfs::vd_context_kind,
                                                            vfs::vd_object_name,
                                                            vfs::vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volumefactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volumefactory_ref));

    ShmIdlInterface::StringSequence_var results;
    std::vector<std::string> volumes;
    volumefactory_ref->list_volumes(results.out());
    for (unsigned int i = 0; i < results->length(); i++)
    {
        volumes.push_back(results[i].in());
    }
    return volumes;
}

int
ShmClient::stat(const std::string& volume_name,
                struct stat *st)
{
    CORBA::Object_var obj = orb_helper().getObjectReference(vfs::vd_context_name,
                                                            vfs::vd_context_kind,
                                                            vfs::vd_object_name,
                                                            vfs::vd_object_kind);
    assert(not CORBA::is_nil(obj));
    ShmIdlInterface::VolumeFactory_var volfactory_ref =
        ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(volfactory_ref));
    uint64_t vol_size = volfactory_ref->stat_volume(volume_name.c_str());

    st->st_blksize = 512;
    st->st_size = vol_size;
    return 0;
}

} //namespace libovsvolumedriver
