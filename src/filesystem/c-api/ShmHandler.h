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

#ifndef __SHM_HANDLER_H
#define __SHM_HANDLER_H

#include "VolumeCacheHandler.h"
#include "ShmControlChannelClient.h"
#include "IOThread.h"
#include "internal.h"

struct ovs_shm_context
{
    ovs_shm_context(const std::string& volume_name, int flag);

    ~ovs_shm_context();

    void
    close_rr_iothreads();

    void
    close_wr_iothreads();

    void
    ovs_aio_init();

    void
    ovs_aio_destroy();

    void
    rr_handler(void *arg);

    void
    wr_handler(void *arg);

    int oflag;
    int io_threads_pool_size_;
    volumedriverfs::ShmClientPtr shm_client_;
    std::vector<IOThreadPtr> rr_iothreads;
    std::vector<IOThreadPtr> wr_iothreads;
    VolumeCacheHandlerPtr cache_;
    ShmControlChannelClientPtr ctl_client_;
};

#endif //SHM_HANDLER_H
