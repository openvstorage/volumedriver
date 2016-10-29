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

#include "NetworkXioIOHandler.h"
#include "NetworkXioProtocol.h"
#include "ObjectRouter.h"
#include "PythonClient.h" // clienterrors

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/SocketAddress.h>

namespace volumedriverfs
{

namespace yt = youtils;

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
NetworkXioIOHandler::update_fs_client_info(const std::string& volume_name)
{
    uint16_t port = 0;
    std::string host;

    xio_connection_attr xcon_peer;
    int ret = xio_query_connection(cd_->conn,
                                   &xcon_peer,
                                   XIO_CONNECTION_ATTR_USER_CTX |
                                   XIO_CONNECTION_ATTR_PEER_ADDR);
    if (ret < 0)
    {
        LOG_ERROR(volume_name << ": failed to query the xio connection: " <<
                  xio_strerror(xio_errno()));
    }
    else
    {
        const yt::SocketAddress sa(xcon_peer.peer_addr);
        port = sa.get_port();
        host = sa.get_ip_address();
    }

    const FrontendPath volume_path(make_volume_path(volume_name));
    cd_->tag = fs_.register_client(ClientInfo(fs_.find_id(volume_path),
                                              std::move(host),
                                              port));
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
        update_fs_client_info(volume_name);
        volume_name_ = volume_name;
        req->retval = 0;
        req->errval = 0;
    }
    catch (const HierarchicalArakoon::DoesNotExistException&)
    {
        LOG_ERROR("volume '" << volume_name << "' doesn't exist");
        req->retval = -1;
        req->errval = ENOENT;
    }
    CATCH_STD_ALL_EWHAT({
       LOG_ERROR("failed to open volume '" << volume_name << "' " << EWHAT);
       req->retval = -1;
       req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_close(NetworkXioRequest *req)
{
    req->op = NetworkXioMsgOpcode::CloseRsp;
    try
    {
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
    }
    CATCH_STD_ALL_EWHAT({
       LOG_ERROR("close I/O error: " << EWHAT);
       req->retval = -1;
       req->errval = EIO;
    });
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
        LOG_INFO("cannot allocate requested buffer from mempool, size: "
                 << size << ", falling back to non-mempool allocation");
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

    xio_msg *xio_req = req->xio_req;
    xio_iovec_ex *isglist = vmsg_sglist(&xio_req->in);
    int inents = vmsg_sglist_nents(&xio_req->in);

    if (inents >= 1)
    {
        req->data = isglist[0].iov_base;
        req->data_len = isglist[0].iov_len;
    }
    else
    {
        LOG_ERROR("inents is '" << inents << "', write I/O error");
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

    const FrontendPath volume_path(make_volume_path(volume_name));
    try
    {
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
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem retrieving volume size: " << EWHAT);
        req->retval = -1;
        req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_truncate(NetworkXioRequest *req,
                                     const std::string& volume_name,
                                     const uint64_t offset)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::TruncateRsp;

    const std::string root_("/");
    const FrontendPath volume_path(root_ + volume_name +
                                   fs_.vdisk_format().volume_suffix());
    try
    {
        fs_.truncate(volume_path,
                     static_cast<off_t>(offset));
        req->retval = 0;
        req->errval = 0;
    }
    catch (const HierarchicalArakoon::DoesNotExistException&)
    {
        req->retval = -1;
        req->errval = ENOENT;
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem truncating volume: " << EWHAT);
        req->retval = -1;
        req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_list_volumes(NetworkXioRequest *req)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::ListVolumesRsp;

    uint64_t total_size = 0;
    std::vector<std::string> volumes;
    try
    {
        auto registry(fs_.object_router().object_registry());
        const auto objs(registry->list());
        for (const auto& o: objs)
        {
            const auto reg(registry->find(o, IgnoreCache::F));
            if (reg and (reg->object().type == ObjectType::Volume or
                reg->object().type == ObjectType::Template))
            {
                try
                {
                    std::string vol(fs_.find_path(reg->volume_id).string());
                    vol.erase(vol.rfind(fs_.vdisk_format().volume_suffix()));
                    vol.erase(0, 1);
                    total_size += vol.length() + 1;
                    volumes.push_back(vol);
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

    if (volumes.empty())
    {
        req->errval = 0;
        req->retval = 0;
        pack_msg(req);
        return;
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

    const FrontendPath volume_path(make_volume_path(volume_name));
    std::vector<std::string> snaps;
    size_t volume_size = 0;
    try
    {
        boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
        if (not volume_id)
        {
            req->retval = -1;
            req->errval = ENOENT;
            pack_msg(req);
            return;
        }
        snaps = fs_.object_router().list_snapshots(*volume_id);
        volume_size = fs_.object_router().get_size(*volume_id);
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

    if (snaps.empty())
    {
        req->errval = 0;
        req->retval = 0;
        req->size = volume_size;
        pack_msg(req);
        return;
    }

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
    req->size = volume_size;
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

    const FrontendPath volume_path(make_volume_path(volume_name));
    const volumedriver::SnapshotName snap(snap_name);
    try
    {
        boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
        if (not volume_id)
        {
            req->retval = -1;
            req->errval = ENOENT;
            pack_msg(req);
            return;
        }
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
    catch (clienterrors::SnapshotNameAlreadyExistsException& e)
    {
        LOG_INFO("Volume still has children: " << volume_name);
        req->retval = -1;
        req->errval = EEXIST;
    }
    catch (clienterrors::PreviousSnapshotNotOnBackendException& e)
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

    const FrontendPath volume_path(make_volume_path(volume_name));
    const volumedriver::SnapshotName snap(snap_name);
    try
    {
        boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
        if (not volume_id)
        {
            req->retval = -1;
            req->errval = ENOENT;
            pack_msg(req);
            return;
        }
        fs_.object_router().delete_snapshot(*volume_id,
                                            snap);
        req->retval = 0;
        req->errval = 0;
    }
    catch (clienterrors::SnapshotNotFoundException& e)
    {
        LOG_INFO("Snapshot not found: " << snap_name);
        req->retval = -1;
        req->errval = ENOENT;
    }
    catch (clienterrors::ObjectStillHasChildrenException& e)
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

    const FrontendPath volume_path(make_volume_path(volume_name));
    const volumedriver::SnapshotName snap(snap_name);
    try
    {
        boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
        if (not volume_id)
        {
            req->retval = -1;
            req->errval = ENOENT;
            pack_msg(req);
            return;
        }
        fs_.object_router().rollback_volume(*volume_id,
                                            snap);
        req->retval = 0;
        req->errval = 0;
    }
    catch (clienterrors::ObjectStillHasChildrenException& e)
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

    const FrontendPath volume_path(make_volume_path(volume_name));
    const volumedriver::SnapshotName snap(snap_name);
    try
    {
        boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
        if (not volume_id)
        {
            req->retval = -1;
            req->errval = ENOENT;
            pack_msg(req);
            return;
        }
        bool is_synced = fs_.object_router().is_volume_synced_up_to(*volume_id,
                                                                    snap);
        req->retval = is_synced;
        req->errval = 0;
    }
    catch (clienterrors::SnapshotNotFoundException& e)
    {
        LOG_INFO("Snapshot not found: " << snap_name);
        req->retval = -1;
        req->errval = ENOENT;
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem checking if snapshot '" << snap_name <<
                  "' is synced for volume " << volume_name << " ,err:"
                  << EWHAT);
        req->retval = -1;
        req->errval = EIO;
    });
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_list_cluster_node_uri(NetworkXioRequest *req)
{
    VERIFY(not handle_);
    req->op = NetworkXioMsgOpcode::ListClusterNodeURIRsp;

    uint64_t total_size = 0;
    std::vector<std::string> uris;
    try
    {
        auto registry(fs_.object_router().cluster_registry());
        const auto configs(registry->get_node_configs());
        for (const auto& c: configs)
        {
            if (c.network_server_uri)
            {
                std::string uri(boost::lexical_cast<std::string>(*c.network_server_uri));
                total_size += uri.length() + 1;
                uris.push_back(uri);
            }
        }
    }
    CATCH_STD_ALL_EWHAT({
        LOG_ERROR("Problem listing cluster nodes URI: " << EWHAT);
        req->retval = -1;
        req->errval = EIO;
        pack_msg(req);
        return;
    });

    if (uris.empty())
    {
        req->errval = 0;
        req->retval = 0;
        pack_msg(req);
        return;
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
    for(auto& uri: uris)
    {
        const char *un = uri.c_str();
        strcpy(static_cast<char*>(req->data) + idx, un);
        idx += strlen(un) + 1;
    }
    req->errval = 0;
    req->retval = uris.size();
    pack_msg(req);
}

void
NetworkXioIOHandler::handle_get_volume_uri(NetworkXioRequest* req,
                                           const std::string& vname)
{
    req->op = NetworkXioMsgOpcode::GetVolumeURIRsp;

    // TODO: move exception conversion to a common helper
    try
    {
        const FrontendPath path(make_volume_path(vname));
        const boost::optional<ObjectId> oid(fs_.find_id(path));
        if (not oid)
        {
            LOG_ERROR("failed to find object ID of " << vname);
            throw fungi::IOException("Failed to find object ID",
                                     vname.c_str(),
                                     ENOENT);
        }

        std::shared_ptr<CachedObjectRegistry> oregistry(fs_.object_router().object_registry());
        const ObjectRegistrationPtr oreg(oregistry->find_throw(*oid, IgnoreCache::T));
        std::shared_ptr<ClusterRegistry> cregistry(fs_.object_router().cluster_registry());
        const ClusterNodeStatus status(cregistry->get_node_status(oreg->node_id));

        if (status.config.network_server_uri)
        {
            const auto uri(boost::lexical_cast<std::string>(*status.config.network_server_uri));

            int ret = xio_mem_alloc(uri.size() + 1, &req->reg_mem);
            if (ret < 0)
            {
                LOG_ERROR("failed to allocate buffer of size " << uri.size() + 1);
                throw std::bad_alloc();
            }

            strcpy(static_cast<char*>(req->reg_mem.addr), uri.c_str());
            req->retval = 1;
            req->data_len = uri.size() + 1;
            req->data = req->reg_mem.addr;
        }
        else
        {
            req->retval = 0;
        }

        req->errval = 0;
    }
    catch (std::bad_alloc&)
    {
        req->errval = ENOMEM;
        req->retval = -1;
    }
    catch (ClusterNodeNotRegisteredException&)
    {
        // something else?
        req->errval = ENOENT;
        req->retval = -1;
    }
    catch (ObjectNotRegisteredException&)
    {
        req->errval = ENOENT;
        req->retval = -1;
    }
    catch (fungi::IOException& e)
    {
        LOG_ERROR("Failed to look up location of " << vname << ": " << e.what());
        req->retval = -1;
        req->errval = e.getErrorCode();
        if (req->errval == 0)
        {
            req->errval = EIO;
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Failed to look up location of " << vname << ": " << EWHAT);
            req->errval = EIO;
            req->retval = -1;
        });

    pack_msg(req);
}

void
NetworkXioIOHandler::handle_error(NetworkXioRequest *req,
                                  NetworkXioMsgOpcode op,
                                  int errval)
{
    req->op = op;
    req->retval = -1;
    req->errval = errval;
    pack_msg(req);
}

void
NetworkXioIOHandler::process_request(NetworkXioRequest *req)
{
    xio_msg *xio_req = req->xio_req;

    NetworkXioMsg i_msg(NetworkXioMsgOpcode::Noop);
    try
    {
        i_msg.unpack_msg(static_cast<char*>(xio_req->in.header.iov_base),
                         xio_req->in.header.iov_len);
    }
    catch (...)
    {
        LOG_ERROR("cannot unpack message");
        handle_error(req,
                     NetworkXioMsgOpcode::ErrorRsp,
                     EBADMSG);
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
        handle_write(req,
                     i_msg.size(),
                     i_msg.offset());
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
    case NetworkXioMsgOpcode::TruncateReq:
    {
        handle_truncate(req,
                        i_msg.volume_name(),
                        i_msg.offset());
        break;
    }
    case NetworkXioMsgOpcode::ListClusterNodeURIReq:
    {
        handle_list_cluster_node_uri(req);
        break;
    }
    case NetworkXioMsgOpcode::GetVolumeURIReq:
    {
        handle_get_volume_uri(req,
                              i_msg.volume_name());
        break;
    }
    default:
        LOG_ERROR("Unknown command");
        handle_error(req,
                     NetworkXioMsgOpcode::ErrorRsp,
                     EIO);
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
