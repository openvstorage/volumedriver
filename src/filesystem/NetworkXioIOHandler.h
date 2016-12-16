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

#ifndef __NETWORK_XIO_IO_HANDLER_H_
#define __NETWORK_XIO_IO_HANDLER_H_

#include "FileSystem.h"
#include "ClusterRegistry.h"

#include "NetworkXioRequest.h"
#include "NetworkXioWorkQueue.h"

#include <volumedriver/ClusterLocation.h>
#include <volumedriver/Types.h>

namespace volumedriverfs
{

class NetworkXioIOHandler
{
public:
    NetworkXioIOHandler(FileSystem& fs,
                        NetworkXioWorkQueuePtr wq,
                        NetworkXioWorkQueuePtr wq_ctrl,
                        NetworkXioClientData* cd,
                        const std::atomic<uint32_t>& max_neighbour_distance)
    : fs_(fs)
    , wq_(wq)
    , wq_ctrl_(wq_ctrl)
    , cd_(cd)
    , max_neighbour_distance_(max_neighbour_distance)
    {}

    ~NetworkXioIOHandler()
    {
        if (handle_)
        {
            fs_.release(std::move(handle_));
        }
    }

    NetworkXioIOHandler(const NetworkXioIOHandler&) = delete;

    NetworkXioIOHandler&
    operator=(const NetworkXioIOHandler&) = delete;

    void
    process_request(NetworkXioRequest *req);

    void
    process_ctrl_request(NetworkXioRequest *req);

    void
    handle_request(NetworkXioRequest *req);

    void
    dispatch_ctrl_request(NetworkXioRequest *req);

    void
    update_fs_client_info(const std::string& volume_name);

    void
    remove_fs_client_info()
    {
        fs_.unregister_client(cd_->tag);
    }

private:
    void handle_open(NetworkXioRequest *req,
                     const std::string& volume_name);

    void handle_close(NetworkXioRequest *req);

    void handle_read(NetworkXioRequest *req,
                     size_t size,
                     uint64_t offset);

    void handle_write(NetworkXioRequest *req,
                      size_t size,
                      uint64_t offset);

    void handle_flush(NetworkXioRequest *req);

    void handle_create_volume(NetworkXioRequest *req,
                              const std::string& volume_name,
                              size_t size);

    void handle_remove_volume(NetworkXioRequest *req,
                              const std::string& volume_name);

    void handle_stat_volume(NetworkXioRequest *req,
                            const std::string& volume_name);

    void handle_truncate(NetworkXioRequest *req,
                         const std::string& volume_name,
                         const uint64_t offset);

    void handle_list_volumes(NetworkXioRequest *req);

    void handle_list_snapshots(NetworkXioRequest *req,
                               const std::string& volume_name);

    void handle_create_snapshot(NetworkXioRequest *req,
                                const std::string& volume_name,
                                const std::string& snap_name,
                                const int64_t timeout);

    void handle_delete_snapshot(NetworkXioRequest *req,
                                const std::string& volume_name,
                                const std::string& snap_name);

    void handle_rollback_snapshot(NetworkXioRequest *req,
                                  const std::string& volume_name,
                                  const std::string& snap_name);

    void handle_is_snapshot_synced(NetworkXioRequest *req,
                                   const std::string& volume_name,
                                   const std::string& snap_name);

    void handle_list_cluster_node_uri(NetworkXioRequest *req);

    void handle_get_volume_uri(NetworkXioRequest *req,
                               const std::string& volume_name);

    void handle_get_cluster_multiplier(NetworkXioRequest *req,
                                       const std::string& volume_name);

    void handle_get_clone_namespace_map(NetworkXioRequest *req,
                                        const std::string& volume_name);

    void handle_get_page(NetworkXioRequest *req,
                         const std::string& volume_name,
                         const uint64_t cluster_address);

    void handle_error(NetworkXioRequest *req,
                      NetworkXioMsgOpcode op,
                      int errval);

    void prepare_ctrl_request(NetworkXioRequest *req);

private:
    DECLARE_LOGGER("NetworkXioIOHandler");

    FileSystem& fs_;
    NetworkXioWorkQueuePtr wq_;
    NetworkXioWorkQueuePtr wq_ctrl_;
    NetworkXioClientData *cd_;
    const std::atomic<uint32_t>& max_neighbour_distance_;

    std::string volume_name_;
    Handle::Ptr handle_;

    std::pair<std::vector<std::string>, size_t>
    get_neighbours(const ClusterRegistry::NeighbourMap&) const;

    std::string
    make_volume_path(const std::string& volume_name)
    {
        const std::string root_("/");
        return (root_ + volume_name + fs_.vdisk_format().volume_suffix());
    }

    std::string
    pack_map(const volumedriver::CloneNamespaceMap& cn);

    std::string
    pack_vector(const std::vector<volumedriver::ClusterLocation>& cl);
};

typedef std::unique_ptr<NetworkXioIOHandler> NetworkXioIOHandlerPtr;

} //namespace volumedriverfs

#endif
