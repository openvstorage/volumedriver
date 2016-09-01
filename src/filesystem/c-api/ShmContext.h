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

#ifndef __SHM_CONTEXT_H
#define __SHM_CONTEXT_H

#include "ShmHandler.h"
#include "../ShmControlChannelProtocol.h"
#include "ShmClient.h"
#include "context.h"

struct ShmContext : public ovs_context_t
{
    std::shared_ptr<ovs_shm_context> shm_ctx_;
    std::string volname_;

    ShmContext();

    ~ShmContext();

    int
    open_volume(const char *volume_name,
                int oflag);

    void
    close_volume();

    int
    create_volume(const char *volume_name,
                  uint64_t size);

    int
    remove_volume(const char *volume_name);

    int
    truncate_volume(const char *volume_name,
                    uint64_t size);

    int
    truncate(uint64_t size);

    int
    snapshot_create(const char *volume_name,
                    const char *snapshot_name,
                    const uint64_t timeout);

    int
    snapshot_rollback(const char *volume_name,
                      const char *snapshot_name);

    int
    snapshot_remove(const char *volume_name,
                    const char *snapshot_name);

    void
    list_snapshots(std::vector<std::string>& snaps,
                   const char *volume_name,
                   uint64_t *size,
                   int *saved_errno);

    int
    is_snapshot_synced(const char *volume_name,
                       const char *snapshot_name);

    int
    list_volumes(std::vector<std::string>& volumes);

    int
    send_read_request(struct ovs_aiocb *ovs_aiocbp,
                      ovs_aio_request *request);

    int
    send_write_request(struct ovs_aiocb *ovs_aiocbp,
                       ovs_aio_request *request);

    int
    send_flush_request(ovs_aio_request *request);

    int
    stat_volume(struct stat *st);

    ovs_buffer_t*
    allocate(size_t size);

    int
    deallocate(ovs_buffer_t *ptr);
};

#endif //__SHM_CONTEXT_H
