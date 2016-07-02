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
#include "ShmIdlInterface.h"
#include "ShmOrbClient.h"
#include "ShmProtocol.h"
#include "ShmSegmentDetails.h"

#include <youtils/Assert.h>
#include <youtils/UUID.h>
#include <youtils/OrbHelper.h>

namespace volumedriverfs
{

namespace yt = youtils;

namespace
{

// Does OmniORB support multiple ORBs by now? Older messages on the mailing list
// say it doesn't so for now we'll use a singleton.
// Do we need locking around OrbHelper calls?
DECLARE_LOGGER("ShmOrbClientUtils");

boost::mutex orb_helper_lock;
std::unique_ptr<yt::OrbHelper> orb_helper_instance;

#define LOCK_ORB_HELPER()                                               \
    boost::lock_guard<decltype(orb_helper_lock)> g(orb_helper_lock)

youtils::OrbHelper&
orb_helper()
{
    LOCK_ORB_HELPER();

    if (not orb_helper_instance)
    {
        orb_helper_instance = std::make_unique<yt::OrbHelper>("ShmOrbClient");
    }

    return *orb_helper_instance;
}

}

void
ShmOrbClient::init()
{
    LOCK_ORB_HELPER();

    VERIFY(not orb_helper_instance);
    orb_helper_instance = std::make_unique<yt::OrbHelper>("ShmOrbClient");
}

void
ShmOrbClient::fini()
{
    LOCK_ORB_HELPER();
    VERIFY(orb_helper_instance);
    orb_helper_instance = nullptr;
}

CORBA::Object_var
ShmOrbClient::get_object_reference_()
{
    CORBA::Object_var obj = orb_helper().getObjectReference(segment_details_.cluster_id,
                                                            vd_context_kind,
                                                            segment_details_.vrouter_id,
                                                            vd_object_kind);

    assert(not CORBA::is_nil(obj));
    return obj;
}

ShmIdlInterface::VolumeFactory_var
ShmOrbClient::get_volume_factory_reference_()
{
    CORBA::Object_var obj = get_object_reference_();
    ShmIdlInterface::VolumeFactory_var vref = ShmIdlInterface::VolumeFactory::_narrow(obj);
    assert(not CORBA::is_nil(vref));

    return vref;
}

ShmOrbClient::ShmOrbClient(const ShmSegmentDetails& segment_details)
    : segment_details_(segment_details)
    , volume_factory_ref_(get_volume_factory_reference_())
{}

std::unique_ptr<ShmIdlInterface::HelloReply>
ShmOrbClient::hello(const std::string& sender_id)
{
    return std::unique_ptr<ShmIdlInterface::HelloReply>(volume_factory_ref_->hello(sender_id.c_str()));
}

std::unique_ptr<ShmClient>
ShmOrbClient::open(const std::string& volname)
{
    ShmIdlInterface::CreateShmArguments args;
    args.cluster_id = segment_details_.cluster_id.c_str();
    args.vrouter_id = segment_details_.vrouter_id.c_str();
    args.volume_name = volname.c_str();

    std::unique_ptr<ShmIdlInterface::CreateResult> res(volume_factory_ref_->create_shm_interface(args));
    assert(youtils::UUID::isUUIDString(res->writerequest_uuid));
    assert(youtils::UUID::isUUIDString(res->writereply_uuid));
    assert(youtils::UUID::isUUIDString(res->readrequest_uuid));
    assert(youtils::UUID::isUUIDString(res->readreply_uuid));

    return std::unique_ptr<ShmClient>(new ShmClient(segment_details_,
                                                    volname,
                                                    *res));
}

void
ShmOrbClient::create_volume(const std::string& volume_name,
                            const uint64_t volume_size)
{
    volume_factory_ref_->create_volume(volume_name.c_str(),
                                       volume_size);
}

void
ShmOrbClient::remove_volume(const std::string& volume_name)
{
    volume_factory_ref_->remove_volume(volume_name.c_str());
}

void
ShmOrbClient::truncate_volume(const std::string& volume_name,
                              const uint64_t volume_size)
{
    volume_factory_ref_->truncate_volume(volume_name.c_str(),
                                         volume_size);
}

void
ShmOrbClient::create_snapshot(const std::string& volume_name,
                              const std::string& snapshot_name,
                              const int64_t timeout)
{
    volume_factory_ref_->create_snapshot(volume_name.c_str(),
                                         snapshot_name.c_str(),
                                         timeout);
}

void
ShmOrbClient::rollback_snapshot(const std::string& volume_name,
                                const std::string& snapshot_name)
{
    volume_factory_ref_->rollback_snapshot(volume_name.c_str(),
                                           snapshot_name.c_str());
}

void
ShmOrbClient::delete_snapshot(const std::string& volume_name,
                              const std::string& snapshot_name)
{
    volume_factory_ref_->delete_snapshot(volume_name.c_str(),
                                         snapshot_name.c_str());
}

std::vector<std::string>
ShmOrbClient::list_snapshots(const std::string& volume_name,
                             uint64_t *size)
{
    ShmIdlInterface::StringSequence_var results;
    CORBA::ULongLong c_size;
    std::vector<std::string> snaps;
    volume_factory_ref_->list_snapshots(volume_name.c_str(),
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
ShmOrbClient::is_snapshot_synced(const std::string& volume_name,
                                 const std::string& snapshot_name)
{
    return volume_factory_ref_->is_snapshot_synced(volume_name.c_str(),
                                                   snapshot_name.c_str());
}

std::vector<std::string>
ShmOrbClient::list_volumes()
{
    ShmIdlInterface::StringSequence_var results;
    std::vector<std::string> volumes;
    volume_factory_ref_->list_volumes(results.out());
    for (unsigned int i = 0; i < results->length(); i++)
    {
        volumes.push_back(results[i].in());
    }
    return volumes;
}

void
ShmOrbClient::stat(const std::string& volume_name,
                   struct stat& st)
{
    uint64_t vol_size = volume_factory_ref_->stat_volume(volume_name.c_str());
    st.st_blksize = 512;
    st.st_size = vol_size;
}

void
ShmOrbClient::stop_volume_(const std::string& vname)
{
    volume_factory_ref_->stop_volume(vname.c_str());
}

}
