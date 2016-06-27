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

#ifndef __SHM_VOLUME_DRIVER_HANDLER_H_
#define __SHM_VOLUME_DRIVER_HANDLER_H_

#include "FileSystem.h"
#include "ShmCommon.h"

#include <boost/interprocess/managed_shared_memory.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <ObjectRouter.h>

#include <volumedriver/SnapshotName.h>

namespace volumedriverfs
{

class ShmVolumeDriverHandler
{
public:
    struct construction_args
    {
        FileSystem& fs;
    };

    ShmVolumeDriverHandler(const ::ShmIdlInterface::CreateShmArguments& args,
                           construction_args& handler_args)
        : fs_(handler_args.fs)
        , shm_segment_(new boost::interprocess::managed_shared_memory(boost::interprocess::open_only,
                                                                      ShmSegmentDetails::Name()))
    {
        open(std::string(args.volume_name));

        LOG_INFO("created a new volume handler for volume '" <<
                args.volume_name << "'");
    }

    explicit ShmVolumeDriverHandler(construction_args& handler_args)
        : fs_(handler_args.fs)
    {
        LOG_INFO("created a new volume handler");
    }

    ~ShmVolumeDriverHandler()
    {
        if (handle_)
        {
            fs_.release(std::move(handle_));
        }
    }

    void
    write(const ShmWriteRequest* request,
          ShmWriteReply* reply)
    {
        VERIFY(handle_);

        LOG_TRACE("write request offset: " << request->offset_in_bytes
                  << ", size: " << request->size_in_bytes);

        auto data = static_cast<const char*>(shm_segment_->get_address_from_handle(request->handle));

        reply->opaque = request->opaque;
        reply->size_in_bytes = request->size_in_bytes;

        bool sync = false;
        try
        {
            fs_.write(*handle_,
                      reply->size_in_bytes,
                      data,
                      request->offset_in_bytes,
                      sync);
            reply->failed = false;
        }
        CATCH_STD_ALL_EWHAT({
            LOG_ERROR("write I/O error: " << EWHAT);
            reply->failed = true;
        });
    }

    bool
    flush()
    {
        VERIFY(handle_);

        LOG_TRACE("Flushing");

        try
        {
            fs_.fsync(*handle_,
                      false);
            return true;
        }
        CATCH_STD_ALL_EWHAT({
            LOG_ERROR("flush I/O error: " << EWHAT);
            return false;
        });
    }

    void
    read(const ShmReadRequest* request,
         ShmReadReply* reply)
    {
        VERIFY(handle_);

        LOG_TRACE("read request offset: " << request->offset_in_bytes
                  << ", size_in_bytes: " << request->size_in_bytes);

        reply->opaque = request->opaque;
        reply->size_in_bytes = request->size_in_bytes;

        auto data = static_cast<char*>(shm_segment_->get_address_from_handle(request->handle));

        try
        {
            bool eof = false;
            fs_.read(*handle_,
                     reply->size_in_bytes,
                     data,
                     request->offset_in_bytes,
                     eof);
            reply->failed = false;
        }
        CATCH_STD_ALL_EWHAT({
            LOG_ERROR("read I/O error: " << EWHAT);
            reply->failed = true;
        });
    }

    uint64_t
    volume_size_in_bytes() const
    {
        VERIFY(handle_);
        return fs_.object_router().get_size(handle_->dentry()->object_id());
    }

    void
    create_volume(const std::string& volume_name,
                  CORBA::ULongLong volume_size_in_bytes)
    {
        LOG_INFO("Create new volume with name: " << volume_name
                 << ", size: " << volume_size_in_bytes);

        VERIFY(not handle_);

        const std::string root_("/");
        const std::string dot_(".");
        const FrontendPath volume_path(root_ + volume_name + dot_ +
                                       fs_.vdisk_format().name());
        try
        {
            volumedriver::VolumeConfig::MetaDataBackendConfigPtr
                                   mdb_config(fs_.make_metadata_backend_config());
            fs_.create_volume(volume_path,
                              std::move(mdb_config),
                              static_cast<uint64_t>(volume_size_in_bytes));
        }
        catch (FileExistsException& e)
        {
            throw ShmIdlInterface::VolumeExists(volume_name.c_str());
        }
        catch(std::exception& e)
        {
            LOG_INFO("Problem creating volume: " << volume_name
                     << ":" << e.what());
            throw;
        }

        try
        {
            open(volume_name);
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to open " << volume_name << ": " << EWHAT);
                remove_volume(volume_name);
                throw;
            });
    }

    void
    remove_volume(const std::string& volume_name)
    {
        LOG_INFO("Removing volume with name: " << volume_name);

        const std::string root_("/");
        const std::string dot_(".");
        const FrontendPath volume_path(root_ + volume_name + dot_ +
                                       fs_.vdisk_format().name());
        try
        {
            fs_.unlink(volume_path);
        }
        catch (HierarchicalArakoon::DoesNotExistException&)
        {
            LOG_INFO("Volume '" << volume_name << "' does not exist");
            throw ShmIdlInterface::VolumeDoesNotExist(volume_name.c_str());
        }
        catch (std::exception& e)
        {
            LOG_INFO("Problem removing volume: " << volume_name
                     << ": " << e.what());
            throw;
        }
    }

    void
    truncate_volume(const std::string& volume_name,
                    const uint64_t length)
    {
        LOG_INFO("Truncate volume with name: " << volume_name);

        const FrontendPath volume_path(make_volume_path(volume_name));
        try
        {
            fs_.truncate(volume_path,
                         static_cast<off_t>(length));
        }
        catch (HierarchicalArakoon::DoesNotExistException&)
        {
            LOG_INFO("Volume '" << volume_name << "' does not exist");
            throw ShmIdlInterface::VolumeDoesNotExist(volume_name.c_str());
        }
        catch (std::exception& e)
        {
            LOG_INFO("Problem removing volume: " << volume_name
                     << ": " << e.what());
            throw;
        }
    }

    uint64_t
    stat_volume(const std::string& volume_name)
    {
        const FrontendPath volume_path(make_volume_path(volume_name));
        boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
        if (not volume_id)
        {
            throw ShmIdlInterface::VolumeDoesNotExist(volume_name.c_str());
        }
        else
        {
            return fs_.object_router().get_size(*volume_id);
        }
    }

    void
    create_snapshot(const std::string& volume_name,
                    const std::string& snap_name,
                    const int64_t timeout)
    {
        LOG_INFO("Create snapshot with name: " << snap_name <<
                 " for volume with name: " << volume_name);

        const FrontendPath volume_path(make_volume_path(volume_name));
        boost::optional<ObjectId> volume_id(get_objectid(volume_path));

        const volumedriver::SnapshotName snap(snap_name);
        try
        {
            fs_.object_router().create_snapshot(*volume_id,
                                                snap,
                                                timeout);
        }
        catch (SyncTimeoutException& e)
        {
            LOG_INFO("Sync timeout exception for volume: " << volume_name);
            throw ShmIdlInterface::SyncTimeoutException(snap_name.c_str());
        }
        catch (volumedriver::SnapshotPersistor::SnapshotNameAlreadyExists& e)
        {
            LOG_INFO("Volume still has children: " << volume_name);
            throw ShmIdlInterface::SnapshotAlreadyExists(snap_name.c_str());
        }
        catch (volumedriver::PreviousSnapshotNotOnBackendException& e)
        {
            LOG_INFO("Previous snapshot not on backend yet for volume: " << volume_name);
            throw ShmIdlInterface::PreviousSnapshotNotOnBackendException(snap_name.c_str());
        }
        catch (std::exception& e)
        {
            LOG_INFO("Problem creating snapshot: " << snap_name <<
                     " for volume: "<<  volume_name << ",err: " << e.what());
            throw;
        }
    }

    void
    rollback_snapshot(const std::string& volume_name,
                      const std::string& snap_name)
    {
        LOG_INFO("Rollback snapshot with name: " << snap_name <<
                 " for volume with name: " << volume_name);

        const FrontendPath volume_path(make_volume_path(volume_name));
        boost::optional<ObjectId> volume_id(get_objectid(volume_path));

        const volumedriver::SnapshotName snap(snap_name);
        try
        {
            fs_.object_router().rollback_volume(*volume_id,
                                                snap);
        }
        catch (ObjectStillHasChildrenException& e)
        {
            LOG_INFO("Volume still has children: " << volume_name);
            throw ShmIdlInterface::VolumeHasChildren(volume_name.c_str());
        }
        catch (std::exception& e)
        {
            LOG_INFO("Problem rolling back snapshot: " << snap_name <<
                     " for volume: "<<  volume_name << ",err: " << e.what());
            throw;
        }
    }

    void
    delete_snapshot(const std::string& volume_name,
                    const std::string& snap_name)
    {
        LOG_INFO("Removing snapshot with name: " << snap_name <<
                 "from volume with name: " << volume_name);

        const FrontendPath volume_path(make_volume_path(volume_name));
        boost::optional<ObjectId> volume_id(get_objectid(volume_path));

        const volumedriver::SnapshotName snap(snap_name);
        try
        {
            fs_.object_router().delete_snapshot(*volume_id,
                                                snap);
        }
        catch (volumedriver::SnapshotNotFoundException& e)
        {
            LOG_INFO("Snapshot not found: " << snap_name);
            throw ShmIdlInterface::SnapshotNotFound(volume_name.c_str());
        }
        catch (ObjectStillHasChildrenException& e)
        {
            LOG_INFO("Volume still has children: " << volume_name);
            throw ShmIdlInterface::VolumeHasChildren(volume_name.c_str());
        }
        catch (std::exception& e)
        {
            LOG_INFO("Problem removing snapshot: " << snap_name <<
                     " for volume: "<<  volume_name << ",err: " << e.what());
            throw;
        }
    }

    std::vector<std::string>
    list_snapshots(const std::string& volume_name,
                   uint64_t *size)
    {
        LOG_INFO("Listing snapshots for volume: " << volume_name);

        const FrontendPath volume_path(make_volume_path(volume_name));
        boost::optional<ObjectId> volume_id(get_objectid(volume_path));
        std::list<volumedriver::SnapshotName> snaps;
        try
        {
            snaps = fs_.object_router().list_snapshots(*volume_id);
        }
        catch (std::exception& e)
        {
            LOG_INFO("Problem listing snapshots for volume " << volume_name <<
                     ",err: " << e.what());
            throw;
        }

        std::vector<std::string> s_snaps;
        for (const auto& s: snaps)
        {
            s_snaps.push_back(s);
        }
        *size = get_volume_size(*volume_id);
        return s_snaps;
    }

    bool
    is_snapshot_synced(const std::string& volume_name,
                       const std::string& snap_name)
    {
        const FrontendPath volume_path(make_volume_path(volume_name));
        boost::optional<ObjectId> volume_id(get_objectid(volume_path));
        const volumedriver::SnapshotName snap(snap_name);
        try
        {
            return fs_.object_router().is_volume_synced_up_to(*volume_id,
                                                              snap);
        }
        catch (volumedriver::SnapshotNotFoundException& e)
        {
            LOG_INFO("Snapshot not found: " << snap_name);
            throw ShmIdlInterface::SnapshotNotFound(volume_name.c_str());
        }
        catch (std::exception& e)
        {
            LOG_INFO("Problem checking if snapshot " << snap_name <<
                    " is synced for volume " << volume_name << ",err: " <<
                    e.what());
            throw;
        }
    }

    std::vector<std::string>
    list_volumes()
    {
        auto registry(fs_.object_router().object_registry());
        const auto objs(registry->list());

        try
        {
            std::vector<std::string> volumes;
            for (const auto& o: objs)
            {
                const auto reg(registry->find(o, IgnoreCache::F));
                if (reg and (reg->object().type == ObjectType::Volume or
                    reg->object().type == ObjectType::Template))
                {
                    try
                    {
                        std::string volume(fs_.find_path(reg->volume_id).string());
                        volume.erase(volume.rfind(fs_.vdisk_format().volume_suffix()));
                        volume.erase(0, 1);
                        volumes.push_back(volume);
                    }
                    catch (HierarchicalArakoon::DoesNotExistException&)
                    {
                        continue;
                    }
                }
            }
            return volumes;
        }
        catch (std::exception& e)
        {
            LOG_INFO("Problem listing volumes, err: " << e.what());
            throw;
        }
    }

private:
    DECLARE_LOGGER("ShmVolumeDriverHandler");

    FileSystem& fs_;
    Handle::Ptr handle_;

    std::unique_ptr<boost::interprocess::managed_shared_memory> shm_segment_;

    boost::optional<ObjectId>
    get_objectid(const FrontendPath& path)
    {
        boost::optional<ObjectId> obj_(fs_.find_id(path));
        if (not obj_)
        {
            throw ShmIdlInterface::VolumeDoesNotExist(path.string().c_str());
        }
        return obj_;
    }

    uint64_t
    get_volume_size(const ObjectId& id)
    {
        return fs_.object_router().get_size(id);
    }

    FrontendPath
    make_volume_path(const std::string& volume_name) const
    {
        const std::string root_("/");
        return FrontendPath(root_ + volume_name +
                            fs_.vdisk_format().volume_suffix());
    }

    void
    open(const std::string& volname)
    {
        VERIFY(not handle_);

        const FrontendPath p(make_volume_path(volname));
        // called for its side effect of checking the object presence
        get_objectid(p);
        fs_.open(p,
                 O_RDWR,
                 handle_);
    }
};

}

#endif // !__SHM_VOLUME_DRIVER_HANDLER_H_
