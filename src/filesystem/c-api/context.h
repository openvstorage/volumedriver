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

#ifndef __CONTEXT_H
#define __CONTEXT_H

#include "common.h"

#include <vector>

struct ovs_aio_request;
struct ovs_aiocb;

struct ovs_context_t
{
    TransportType transport;
    int oflag;

    virtual ~ovs_context_t() {};

    virtual int open_volume(const char *volume_name,
                            int oflag) = 0;

    virtual void close_volume() = 0;

    virtual int create_volume(const char *volume_name,
                              uint64_t size) = 0;

    virtual int remove_volume(const char *volume_name) = 0;

    virtual int snapshot_create(const char *volume_name,
                                const char *snapshot_name,
                                const uint64_t timeout) = 0;

    virtual int snapshot_rollback(const char *volume_name,
                                  const char *snapshot_name) = 0;

    virtual int snapshot_remove(const char *volume_name,
                                const char *snapshot_name) = 0;

    virtual void list_snapshots(std::vector<std::string>& snaps,
                                const char *volume_name,
                                uint64_t *size,
                                int *saved_errno) = 0;

    virtual int is_snapshot_synced(const char *volume_name,
                                   const char *snapshot_name) = 0;

    virtual int list_volumes(std::vector<std::string>& volumes) = 0;

    virtual int list_cluster_node_uri(std::vector<std::string>& uris) = 0;

    virtual int get_volume_uri(const char* volume_name,
                               std::string& uri) = 0;

    virtual int send_read_request(struct ovs_aiocb *ovs_aiocbp,
                                  ovs_aio_request *request) = 0;

    virtual int send_write_request(struct ovs_aiocb *ovs_aiocbp,
                                   ovs_aio_request *request) = 0;

    virtual int send_flush_request(ovs_aio_request *request) = 0;

    virtual int stat_volume(struct stat *st) = 0;

    virtual ovs_buffer* allocate(size_t size) = 0;

    virtual int deallocate(ovs_buffer *ptr) = 0;

    virtual int truncate_volume(const char *volume_name,
                                uint64_t length) = 0;

    virtual int truncate(uint64_t length) = 0;
};

#endif // __CONTEXT_H
