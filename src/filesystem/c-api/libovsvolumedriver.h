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
//
//
#ifndef __LIB_OVS_VOLUMEDRIVER_H
#define __LIB_OVS_VOLUMEDRIVER_H


#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#include "common.h"
#include "VolumeCacheHandler.h"

#include <boost/asio.hpp>

typedef struct _ovs_async_threads
{
    ovs_iothread_t *rr_iothread;
    ovs_iothread_t *wr_iothread;
} ovs_async_threads;

struct _ovs_ctx_t
{
    void *shm_handle_;
    int oflag;
    ovs_async_threads async_threads_;
    VolumeCacheHandlerPtr cache_;
    std::unique_ptr<boost::asio::io_service> io_service_;
    std::unique_ptr<boost::asio::local::stream_protocol::socket> socket_;
};


#endif
