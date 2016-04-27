// Copyright 2016 iNuron NV
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
//

#include <youtils/Assert.h>
#include <youtils/Catchers.h>

#include <ObjectRouter.h>

#include "NetworkXioIOHandler.h"
#include "NetworkXioProtocol.h"

namespace volumedriverfs
{

static inline void
pack_msg(NetworkXioRequest *req)
{
    NetworkXioMsg o_msg(req->op);
    o_msg.retval(req->retval);
    o_msg.errval(req->errval);
    o_msg.opaque(req->opaque);
    req->s_msg= o_msg.pack_msg();
}

void
NetworkXioIOHandler::handle_open(NetworkXioRequest *req,
                                 const std::string& volume_name)
{
    LOG_DEBUG("trying to open volume with name '" << volume_name  << "'");

    req->op = NetworkXioMsgOpcode::OpenRsp;
    if (handle_)
    {
        LOG_ERROR("volume '" << volume_name <<
                  "' is already open for this session");
        req->retval = -1;
        req->errval = EIO;
        pack_msg(req);
        return;
    }

    const FrontendPath p("/" + volume_name +
                         fs_.vdisk_format().volume_suffix());

    try
    {
        fs_.open(p, O_RDWR, handle_);
        volume_name_ = volume_name;
        req->retval = 0;
        req->errval = 0;
    }
    catch (const HierarchicalArakoon::DoesNotExistException&)
    {
        LOG_ERROR("volume '" << volume_name << "' doesn't exist");
        req->retval = -1;
        req->errval = EACCES;
    }
    catch (...)
    {
        LOG_ERROR("failed to open volume '" << volume_name << "'");
        req->retval = -1;
        req->errval = EIO;
    }
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_close(NetworkXioRequest *req)
{
    req->op = NetworkXioMsgOpcode::CloseRsp;
    if (handle_)
    {
        fs_.release(std::move(handle_));
        req->retval = 0;
        req->errval = 0;
    }
    else
    {
        LOG_ERROR("failed to close volume '" << volume_name_ << "'");
        req->retval = -1;
        req->errval = EIO;
    }
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_read(NetworkXioRequest *req,
                                 size_t size,
                                 uint64_t offset)
{
    req->op = NetworkXioMsgOpcode::ReadRsp;
    if (not handle_)
    {
        req->retval = -1;
        req->errval = EIO;
        pack_msg(req);
        return;
    }

    int ret = xio_mempool_alloc(req->cd->mpool, size, &req->reg_mem);
    if (ret < 0)
    {
        ret = xio_mem_alloc(size, &req->reg_mem);
        if (ret < 0)
        {
            LOG_ERROR("cannot allocate requested buffer, size: " << size);
            req->retval = -1;
            req->errval = ENOMEM;
            pack_msg(req);
            return;
        }
        req->from_pool = false;
    }

    req->data = req->reg_mem.addr;
    req->data_len = size;
    req->size = size;
    req->offset = offset;
    try
    {
       bool eof = false;
       fs_.read(*handle_,
                req->size,
                static_cast<char*>(req->data),
                req->offset,
                eof);
       req->retval = req->size;
       req->errval = 0;
    }
    CATCH_STD_ALL_EWHAT({
       LOG_ERROR("read I/O error: " << EWHAT);
       req->retval = -1;
       req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_write(NetworkXioRequest *req,
                                  size_t size,
                                  uint64_t offset)
{
    req->op = NetworkXioMsgOpcode::WriteRsp;
    if (not handle_)
    {
        req->retval = -1;
        req->errval = EIO;
        pack_msg(req);
        return;
    }

    if (req->data_len < size)
    {
       LOG_ERROR("data buffer size is smaller than the requested write size"
                 " for volume '" << volume_name_ << "'");
       req->retval = -1;
       req->errval = EIO;
       pack_msg(req);
       return;
    }

    req->size = size;
    req->offset = offset;
    bool sync = false;
    try
    {
       fs_.write(*handle_,
                 req->size,
                 static_cast<char*>(req->data),
                 req->offset,
                 sync);
       req->retval = req->size;
       req->errval = 0;
    }
    CATCH_STD_ALL_EWHAT({
       LOG_ERROR("write I/O error: " << EWHAT);
       req->retval = -1;
       req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_flush(NetworkXioRequest *req)
{
    req->op = NetworkXioMsgOpcode::FlushRsp;
    if (not handle_)
    {
        req->retval = -1;
        req->errval = EIO;
        pack_msg(req);
        return;
    }

    LOG_TRACE("Flushing");
    try
    {
       fs_.fsync(*handle_, false);
       req->retval = 0;
       req->errval = 0;
    }
    CATCH_STD_ALL_EWHAT({
       LOG_ERROR("flush I/O error: " << EWHAT);
       req->retval = -1;
       req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_create_volume(NetworkXioRequest *req,
                                          const std::string& volume_name,
                                          size_t size)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::CreateVolumeRsp;

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
                          static_cast<uint64_t>(size));
        req->retval = 0;
        req->errval = 0;
    }
    catch (const FileExistsException& e)
    {
        req->retval = -1;
        req->errval = EEXIST;
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem creating volume: " << EWHAT);
        req->retval = -1;
        req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_remove_volume(NetworkXioRequest *req,
                                          const std::string& volume_name)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::RemoveVolumeRsp;

    const std::string root_("/");
    const std::string dot_(".");
    const FrontendPath volume_path(root_ + volume_name + dot_ +
                                   fs_.vdisk_format().name());
    try
    {
        fs_.unlink(volume_path);
    }
    catch (const HierarchicalArakoon::DoesNotExistException&)
    {
        req->retval = -1;
        req->errval = ENOENT;
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem removing volume: " << EWHAT);
        req->retval = -1;
        req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_stat_volume(NetworkXioRequest *req,
                                        const std::string& volume_name)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::StatVolumeRsp;

    const std::string root_("/");
    const std::string dot_(".");
    const FrontendPath volume_path(root_ + volume_name +
                                   fs_.vdisk_format().volume_suffix());
    boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
    if (not volume_id)
    {
        req->retval = -1;
        req->errval = ENOENT;
    }
    else
    {
        req->retval = fs_.object_router().get_size(*volume_id);
        req->errval = 0;
    }
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_list_volumes(NetworkXioRequest *req)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::ListVolumesRsp;

    auto registry(fs_.object_router().object_registry());
    const auto objs(registry->list());

    uint64_t total_size = 0;
    std::vector<std::string> volumes;
    try
    {
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
                    total_size += volume.length() + 1;
                    volumes.push_back(volume);
                }
                catch (HierarchicalArakoon::DoesNotExistException&)
                {
                    continue;
                }
            }
        }
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem listing volumes: " << EWHAT);
        req->retval = -1;
        req->errval = EIO;
        pack_msg(req);
        return;
    });
    int ret = xio_mem_alloc(total_size, &req->reg_mem);
    if (ret < 0)
    {
        LOG_ERROR("cannot allocate requested buffer, size: " << total_size);
        req->retval = -1;
        req->errval = ENOMEM;
        pack_msg(req);
        return;
    }
    req->from_pool = false;
    req->data = req->reg_mem.addr;
    req->data_len = total_size;
    uint64_t idx = 0;
    for(auto& vol: volumes)
    {
        const char *name = vol.c_str();
        strcpy(static_cast<char*>(req->data) + idx, name);
        idx += strlen(name) + 1;
    }
    req->errval = 0;
    req->retval = volumes.size();
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_list_snapshots(NetworkXioRequest *req,
                                           const std::string& volume_name)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::ListSnapshotsRsp;

    const std::string root_("/");
    const std::string dot_(".");
    const FrontendPath volume_path(root_ + volume_name +
                                   fs_.vdisk_format().volume_suffix());
    boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
    if (not volume_id)
    {
        req->retval = -1;
        req->errval = ENOENT;
        pack_msg(req);
        return;
    }

    std::list<volumedriver::SnapshotName> snaps;
    try
    {
        snaps = fs_.object_router().list_snapshots(*volume_id);
        req->retval = 0;
        req->errval = 0;
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem listing snapshots for volume " << volume_name
                  << " ,err:" << EWHAT);
        req->retval = -1;
        req->errval = EIO;
        pack_msg(req);
        return;
    });

    uint64_t total_size = 0;
    for (const auto& s: snaps)
    {
        total_size += s.length() + 1;
    }

    int ret = xio_mem_alloc(total_size, &req->reg_mem);
    if (ret < 0)
    {
        LOG_ERROR("cannot allocate requested buffer, size: " << total_size);
        req->retval = -1;
        req->errval = ENOMEM;
        pack_msg(req);
        return;
    }
    req->from_pool = false;
    req->data = req->reg_mem.addr;
    req->data_len = total_size;
    uint64_t idx = 0;
    for(auto& s: snaps)
    {
        const char *name = s.c_str();
        strcpy(static_cast<char*>(req->data) + idx, name);
        idx += strlen(name) + 1;
    }
    req->errval = 0;
    req->retval = snaps.size();
    req->size = fs_.object_router().get_size(*volume_id);
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_create_snapshot(NetworkXioRequest *req,
                                            const std::string& volume_name,
                                            const std::string& snap_name,
                                            const int64_t timeout)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::CreateSnapshotRsp;

    const std::string root_("/");
    const std::string dot_(".");
    const FrontendPath volume_path(root_ + volume_name +
                                   fs_.vdisk_format().volume_suffix());
    boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
    if (not volume_id)
    {
        req->retval = -1;
        req->errval = ENOENT;
        pack_msg(req);
        return;
    }

    const volumedriver::SnapshotName snap(snap_name);
    try
    {
        fs_.object_router().create_snapshot(*volume_id,
                                            snap,
                                            timeout);
        req->retval = 0;
        req->errval = 0;
    }
    catch (SyncTimeoutException& e)
    {
        LOG_INFO("Sync timeout exception for snapshotting volume: " <<
                  volume_name);
        req->retval = -1;
        req->errval = ETIMEDOUT;
    }
    catch (volumedriver::SnapshotPersistor::SnapshotNameAlreadyExists& e)
    {
        LOG_INFO("Volume still has children: " << volume_name);
        req->retval = -1;
        req->errval = EEXIST;
    }
    catch (volumedriver::PreviousSnapshotNotOnBackendException& e)
    {
        LOG_INFO("Previous snapshot not on backend yet for volume: " <<
                 volume_name);
        req->retval = -1;
        req->errval = EBUSY;
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem creating snapshot for volume " << volume_name
                  << " ,err:" << EWHAT);
        req->retval = -1;
        req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_delete_snapshot(NetworkXioRequest *req,
                                            const std::string& volume_name,
                                            const std::string& snap_name)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::DeleteSnapshotRsp;

    const std::string root_("/");
    const std::string dot_(".");
    const FrontendPath volume_path(root_ + volume_name +
                                   fs_.vdisk_format().volume_suffix());
    boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
    if (not volume_id)
    {
        req->retval = -1;
        req->errval = ENOENT;
        pack_msg(req);
        return;
    }

    const volumedriver::SnapshotName snap(snap_name);
    try
    {
        fs_.object_router().delete_snapshot(*volume_id,
                                            snap);
        req->retval = 0;
        req->errval = 0;
    }
    catch (volumedriver::SnapshotNotFoundException& e)
    {
        LOG_INFO("Snapshot not found: " << snap_name);
        req->retval = -1;
        req->errval = ENOENT;
    }
    catch (ObjectStillHasChildrenException& e)
    {
        LOG_INFO("Volume still has children: " << volume_name);
        req->retval = -1;
        req->errval = ENOTEMPTY;
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem removing snapshot '" << snap_name <<
                  "' for volume " << volume_name << " ,err:" << EWHAT);
        req->retval = -1;
        req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_rollback_snapshot(NetworkXioRequest *req,
                                              const std::string& volume_name,
                                              const std::string& snap_name)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::RollbackSnapshotRsp;

    const std::string root_("/");
    const std::string dot_(".");
    const FrontendPath volume_path(root_ + volume_name +
                                   fs_.vdisk_format().volume_suffix());
    boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
    if (not volume_id)
    {
        req->retval = -1;
        req->errval = ENOENT;
        pack_msg(req);
        return;
    }

    const volumedriver::SnapshotName snap(snap_name);
    try
    {
        fs_.object_router().rollback_volume(*volume_id,
                                            snap);
        req->retval = 0;
        req->errval = 0;
    }
    catch (ObjectStillHasChildrenException& e)
    {
        LOG_INFO("Volume still has children: " << volume_name);
        req->retval = -1;
        req->errval = ENOTEMPTY;
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem rolling back snapshot '" << snap_name <<
                  "' for volume " << volume_name << " ,err:" << EWHAT);
        req->retval = -1;
        req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_is_snapshot_synced(NetworkXioRequest *req,
                                               const std::string& volume_name,
                                               const std::string& snap_name)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::IsSnapshotSyncedRsp;

    const std::string root_("/");
    const std::string dot_(".");
    const FrontendPath volume_path(root_ + volume_name +
                                   fs_.vdisk_format().volume_suffix());
    boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
    if (not volume_id)
    {
        req->retval = -1;
        req->errval = ENOENT;
        pack_msg(req);
        return;
    }

    const volumedriver::SnapshotName snap(snap_name);
    try
    {
        bool is_synced = fs_.object_router().is_volume_synced_up_to(*volume_id,
                                                                    snap);
        req->retval = is_synced;
        req->errval = 0;
    }
    catch (volumedriver::SnapshotNotFoundException& e)
    {
        LOG_INFO("Snapshot not found: " << snap_name);
        req->retval = -1;
        req->errval = ENOENT;
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem checking if snapshot '" << snap_name <<
                  "' is synced for volume " << volume_name << " ,err:" << EWHAT);
        req->retval = -1;
        req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_error(NetworkXioRequest *req, int errval)
{
    req->op = NetworkXioMsgOpcode::ErrorRsp;
    req->retval = -1;
    req->errval = errval;
    pack_msg(req);
}

void
NetworkXioIOHandler::process_request(NetworkXioRequest *req)
{
    xio_msg *xio_req = req->xio_req;
    xio_iovec_ex *isglist = vmsg_sglist(&xio_req->in);
    int inents = vmsg_sglist_nents(&xio_req->in);

    req->cd->refcnt++;
    NetworkXioMsg i_msg(NetworkXioMsgOpcode::Noop);
    try
    {
        i_msg.unpack_msg(static_cast<char*>(xio_req->in.header.iov_base),
                         xio_req->in.header.iov_len);
    }
    catch (...)
    {
        LOG_ERROR("cannot unpack message");
        handle_error(req, EBADMSG);
        return;
    }

    req->opaque = i_msg.opaque();
    switch (i_msg.opcode())
    {
    case NetworkXioMsgOpcode::OpenReq:
    {
        handle_open(req,
                    i_msg.volume_name());
        break;
    }
    case NetworkXioMsgOpcode::CloseReq:
    {
        handle_close(req);
        break;
    }
    case NetworkXioMsgOpcode::ReadReq:
    {
        handle_read(req,
                    i_msg.size(),
                    i_msg.offset());
        break;
    }
    case NetworkXioMsgOpcode::WriteReq:
    {
        if (inents >= 1)
        {
            req->data = isglist[0].iov_base;
            req->data_len = isglist[0].iov_len;
            handle_write(req, i_msg.size(), i_msg.offset());
        }
        else
        {
            LOG_ERROR("inents is smaller than 1, cannot proceed with write I/O");
            req->op = NetworkXioMsgOpcode::WriteRsp;
            req->retval = -1;
            req->errval = EIO;
        }
        break;
    }
    case NetworkXioMsgOpcode::FlushReq:
    {
        handle_flush(req);
        break;
    }
    case NetworkXioMsgOpcode::CreateVolumeReq:
    {
        handle_create_volume(req,
                             i_msg.volume_name(),
                             i_msg.size());
        break;
    }
    case NetworkXioMsgOpcode::RemoveVolumeReq:
    {
        handle_remove_volume(req,
                             i_msg.volume_name());
        break;
    }
    case NetworkXioMsgOpcode::StatVolumeReq:
    {
        handle_stat_volume(req,
                           i_msg.volume_name());
        break;
    }
    case NetworkXioMsgOpcode::ListVolumesReq:
    {
        handle_list_volumes(req);
        break;
    }
    case NetworkXioMsgOpcode::ListSnapshotsReq:
    {
        handle_list_snapshots(req,
                              i_msg.volume_name());
        break;
    }
    case NetworkXioMsgOpcode::CreateSnapshotReq:
    {
        handle_create_snapshot(req,
                               i_msg.volume_name(),
                               i_msg.snap_name(),
                               i_msg.timeout());
        break;
    }
    case NetworkXioMsgOpcode::DeleteSnapshotReq:
    {
        handle_delete_snapshot(req,
                               i_msg.volume_name(),
                               i_msg.snap_name());
        break;
    }
    case NetworkXioMsgOpcode::RollbackSnapshotReq:
    {
        handle_rollback_snapshot(req,
                                 i_msg.volume_name(),
                                 i_msg.snap_name());
        break;
    }
    case NetworkXioMsgOpcode::IsSnapshotSyncedReq:
    {
        handle_is_snapshot_synced(req,
                                  i_msg.volume_name(),
                                  i_msg.snap_name());
        break;
    }
    default:
        LOG_ERROR("Unknown command");
        handle_error(req, EIO);
        return;
    };
}

void
NetworkXioIOHandler::handle_request(NetworkXioRequest *req)
{
    req->work.func = std::bind(&NetworkXioIOHandler::process_request,
                               this,
                               req);
    wq_->work_schedule(req);
}

} //namespace volumedriverfs
