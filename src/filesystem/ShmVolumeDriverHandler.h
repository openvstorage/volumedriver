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

#ifndef __SHM_VOLUME_DRIVER_HANDLER_H_
#define __SHM_VOLUME_DRIVER_HANDLER_H_

#include "ShmCommon.h"

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <ObjectRouter.h>
#include <FileSystem.h>
#include <volumedriver/Api.h>
#include <volumedriver/SnapshotName.h>
#include <boost/interprocess/managed_shared_memory.hpp>

namespace volumedriverfs
{

class ShmVolumeDriverHandler
{
public:
    struct shmvolumedriver_construction_args
    {
        FileSystem& fs;
    };

    typedef struct shmvolumedriver_construction_args construction_args;

    ShmVolumeDriverHandler(const ::ShmIdlInterface::CreateShmArguments& args,
                           construction_args& handler_args)
        : fs_(handler_args.fs)
        , objectid_(*get_objectid(make_volume_path(std::string(args.volume_name))))
        , volume_size_in_bytes_(get_volume_size(objectid_))
        , shm_segment_(new boost::interprocess::managed_shared_memory(boost::interprocess::open_only,
                                                                      ShmSegmentDetails::Name()))
    {
        LOG_INFO("created a new volume handler for volume '" <<
                args.volume_name << "'");
    }

    ShmVolumeDriverHandler(construction_args& handler_args)
        : fs_(handler_args.fs)
        , volume_size_in_bytes_(0)
    {
        LOG_INFO("created a new volume handler");
    }

    ~ShmVolumeDriverHandler()
    {
        shm_segment_.reset();
    }

    void
    write(const ShmWriteRequest* request,
          ShmWriteReply* reply)
    {
        LOG_TRACE("write request offset: " << request->offset_in_bytes
                  << ", size: " << request->size_in_bytes);

        uint8_t *data = static_cast<uint8_t*>(shm_segment_->get_address_from_handle(request->handle));

        reply->opaque = request->opaque;
        reply->size_in_bytes = request->size_in_bytes;

        try
        {
            fs_.object_router().write(objectid_,
                                      data,
                                      reply->size_in_bytes,
                                      request->offset_in_bytes);
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
        LOG_TRACE("Flushing");
        try
        {
            fs_.object_router().sync(objectid_);
        }
        CATCH_STD_ALL_EWHAT({
            LOG_ERROR("flush I/O error: " << EWHAT);
            return false;
        });
        return true;
    }

    void
    read(const ShmReadRequest* request,
         ShmReadReply* reply)
    {
        LOG_TRACE("read request offset: " << request->offset_in_bytes
                  << ", size_in_bytes: " << request->size_in_bytes);

        reply->opaque = request->opaque;
        reply->size_in_bytes = request->size_in_bytes;

        uint8_t *data = static_cast<uint8_t*>(shm_segment_->get_address_from_handle(request->handle));

        try
        {
            fs_.object_router().read(objectid_,
                                     data,
                                     reply->size_in_bytes,
                                     request->offset_in_bytes);
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
        return volume_size_in_bytes_;
    }

    void
    create_volume(const std::string& volume_name,
                  CORBA::ULongLong volume_size_in_bytes)
    {
        LOG_INFO("Create new volume with name: " << volume_name
                 << ", size: " << volume_size_in_bytes);

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
                if (reg->object().type == ObjectType::Volume or
                    reg->object().type == ObjectType::Template)
                {
                    std::string volume(fs_.find_path(reg->volume_id).string());
                    volume.erase(volume.rfind(fs_.vdisk_format().volume_suffix()));
                    volume.erase(0, 1);
                    volumes.push_back(volume);
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
    const ObjectId objectid_;
    const uint64_t volume_size_in_bytes_;

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
    make_volume_path(const std::string& volume_name)
    {
        const std::string root_("/");
        return FrontendPath(root_ + volume_name +
                            fs_.vdisk_format().volume_suffix());
    }
};

}
#endif
