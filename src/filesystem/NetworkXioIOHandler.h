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

#include "NetworkXioWorkQueue.h"
#include "NetworkXioRequest.h"

namespace volumedriverfs
{

class NetworkXioIOHandler
{
public:
    NetworkXioIOHandler(FileSystem& fs,
                        NetworkXioWorkQueuePtr wq,
                        NetworkXioClientData* cd)
    : fs_(fs)
    , wq_(wq)
    , cd_(cd)
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
    handle_request(NetworkXioRequest* req);

    void
    update_fs_client_info(const std::string& volume_name)
    {
        char hoststr[NI_MAXHOST], portstr[NI_MAXSERV];
        xio_connection_attr xcon_peer;
        (void) xio_query_connection(cd_->conn,
                                    &xcon_peer,
                                    XIO_CONNECTION_ATTR_USER_CTX |
                                    XIO_CONNECTION_ATTR_PEER_ADDR);
        /* Get peer info */
        getnameinfo((struct sockaddr *)&xcon_peer.peer_addr,
                     sizeof(struct sockaddr_storage),
                     hoststr, sizeof(hoststr),
                     portstr, sizeof(portstr),
                     NI_NUMERICHOST | NI_NUMERICSERV);

        const FrontendPath volume_path(make_volume_path(volume_name));
        boost::optional<ObjectId> volume_id(fs_.find_id(volume_path));
        std::string info(*volume_id +
                         ":" +
                         std::string(hoststr) +
                         ":" +
                         std::string(portstr));
        fs_.insert_xio_cdata(cd_, info);
    }

    void
    remove_fs_client_info()
    {
        fs_.remove_xio_cdata(cd_);
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

    void handle_error(NetworkXioRequest *req,
                      NetworkXioMsgOpcode op,
                      int errval);

private:
    DECLARE_LOGGER("NetworkXioIOHandler");

    FileSystem& fs_;
    NetworkXioWorkQueuePtr wq_;
    NetworkXioClientData *cd_;

    std::string volume_name_;
    Handle::Ptr handle_;

    std::string
    make_volume_path(const std::string& volume_name)
    {
        const std::string root_("/");
        return (root_ + volume_name + fs_.vdisk_format().volume_suffix());
    }
};

typedef std::unique_ptr<NetworkXioIOHandler> NetworkXioIOHandlerPtr;

} //namespace volumedriverfs

#endif
