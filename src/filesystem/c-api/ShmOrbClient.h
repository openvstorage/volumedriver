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

#ifndef __SHM_ORB_CLIENT_H_
#define __SHM_ORB_CLIENT_H_

#include "ShmIdlInterface.h"

#include "../ShmSegmentDetails.h"

#include <youtils/Logging.h>
#include <youtils/OrbHelper.h>

namespace volumedriverfs
{

class ShmClient;

class ShmOrbClient
    : public std::enable_shared_from_this<ShmOrbClient>
{
public:
    explicit ShmOrbClient(const ShmSegmentDetails&);

    ~ShmOrbClient() = default;

    std::unique_ptr<ShmIdlInterface::HelloReply>
    hello(const std::string& sender_id);

    std::unique_ptr<ShmClient>
    open(const std::string& volname);

    void
    create_volume(const std::string& volume_name,
                  const uint64_t volume_size);

    void
    remove_volume(const std::string& volume_name);

    void
    truncate_volume(const std::string& volume_name,
                    const uint64_t volume_size);

    void
    create_snapshot(const std::string& volume_name,
                    const std::string& snapshot_name,
                    const int64_t timeout);

    void
    rollback_snapshot(const std::string& volume_name,
                      const std::string& snapshot_name);

    void
    delete_snapshot(const std::string& volume_name,
                    const std::string& snapshot_name);

    std::vector<std::string>
    list_snapshots(const std::string& volume_name,
                   uint64_t *size);

    int
    is_snapshot_synced(const std::string& volume_name,
                       const std::string& snapshot_name);

    std::vector<std::string>
    list_volumes();

    void
    stat(const std::string& volume_name,
         struct stat&);

    static void
    init();

    static void
    fini();

    const ShmSegmentDetails&
    shm_segment_details() const
    {
        return segment_details_;
    }

private:
    friend class ShmClient;

    const ShmSegmentDetails segment_details_;

    ShmIdlInterface::VolumeFactory_var volume_factory_ref_;

    CORBA::Object_var
    get_object_reference_();

    ShmIdlInterface::VolumeFactory_var
    get_volume_factory_reference_();

    void
    stop_volume_(const std::string&);
};

} //namespace volumedriverfs

#endif
