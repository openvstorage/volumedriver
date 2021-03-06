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

#ifndef __NETWORK_XIO_CONTEXT_H
#define __NETWORK_XIO_CONTEXT_H

#include "NetworkHAContext.h"
#include "NetworkXioClient.h"
#include "common_priv.h"
#include "context.h"

namespace libovsvolumedriver
{

class NetworkXioContext
    : public ovs_context_t
{
public:
    NetworkXioContext(const std::string& uri,
                      uint64_t net_client_qdepth,
                      NetworkHAContext& ha_ctx);

    ~NetworkXioContext();

    int
    open_volume(const char *volume_name,
                int oflag) override final;

    void
    close_volume() override final;

    int
    create_volume(const char *volume_name,
                  uint64_t size) override final;

    int
    truncate_volume(const char *volume_name,
                    uint64_t offset) override final;

    int
    truncate(uint64_t offset) override final;

    int
    remove_volume(const char *volume_name) override final;

    int
    snapshot_create(const char *volume_name,
                    const char *snapshot_name,
                    const uint64_t timeout) override final;

    int
    snapshot_rollback(const char *volume_name,
                      const char *snapshot_name) override final;

    int
    snapshot_remove(const char *volume_name,
                    const char *snapshot_name) override final;

    void
    list_snapshots(std::vector<std::string>& snaps,
                   const char *volume_name,
                   uint64_t *size,
                   int *saved_errno) override final;

    int
    is_snapshot_synced(const char *volume_name,
                       const char *snapshot_name) override final;

    int
    list_volumes(std::vector<std::string>& volumes) override final;

    int
    list_cluster_node_uri(std::vector<std::string>& uris) override final;

    int
    get_volume_uri(const char *volume_name,
                   std::string& uri) override final;

    int
    get_cluster_multiplier(const char *volume_name,
                           uint32_t *cluster_multiplier) override final;

    int
    get_clone_namespace_map(const char *volume_name,
                            CloneNamespaceMap& cn) override final;

    int
    get_page(const char *volume_name,
             const ClusterAddress ca,
             ClusterLocationPage& cl) override final;

    int
    send_read_request(ovs_aio_request*) override final;

    int
    send_write_request(ovs_aio_request*) override final;

    int
    send_flush_request(ovs_aio_request*) override final;

    int
    stat_volume(struct stat *st) override final;

    ovs_buffer_t*
    allocate(size_t size) override final;

    int
    deallocate(ovs_buffer_t *ptr) override final;

    bool
    is_dtl_in_sync() override final;

private:
    std::string uri_;
    uint64_t net_client_qdepth_;
    std::string volname_;
    NetworkHAContext& ha_ctx_;
    libovsvolumedriver::NetworkXioClientPtr net_client_;

    typedef std::shared_ptr<ovs_aio_request> ovs_aio_request_ptr;

    int
    aio_suspend(ovs_aio_request_ptr request);

    ssize_t
    aio_return(ovs_aio_request_ptr request);

    ssize_t
    wait_aio_request(ovs_aio_request_ptr request);
};

} //namespace libovsvolumedriver
#endif //__NETWORK_XIO_CONTEXT_H
