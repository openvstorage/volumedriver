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

#ifndef __SHM__VOLUME_DRIVER_HANDLER_H_
#define __SHM_VOLUME_DRIVER_HANDLER_H_

#include <youtils/Assert.h>
#include <ObjectRouter.h>
#include <FileSystem.h>
#include <volumedriver/Api.h>
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
                                                                      "openvstorage_segment"))
    {
        LOG_INFO("created a new volume handler for volume '" <<
                args.volume_name << "'");
    }

    ShmVolumeDriverHandler(construction_args& handler_args)
        : fs_(handler_args.fs)
        , volume_size_in_bytes_(0)
    {
        LOG_INFO("created a handler for creating a volume");
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

        fs_.object_router().write(objectid_,
                                  data,
                                  reply->size_in_bytes,
                                  request->offset_in_bytes);
    }

    void
    flush()
    {
        LOG_TRACE("Flushing");
        fs_.object_router().sync(objectid_);
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

        fs_.object_router().read(objectid_,
                                 data,
                                 reply->size_in_bytes,
                                 request->offset_in_bytes);
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

    DECLARE_LOGGER("ShmVolumeDriverHandler");

private:
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
