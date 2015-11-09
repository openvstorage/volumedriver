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

struct _ovs_ctx_t
{
    void *shm_handle_;
    int oflag;
};

typedef struct _ovs_iothread
{
    pthread_t io_t;
    pthread_mutex_t io_mutex;
    pthread_cond_t io_cond;
    bool stopped;
    bool stopping;
} ovs_iothread_t;

typedef struct _ovs_async_threads
{
    ovs_iothread_t *rr_iothread;
    ovs_iothread_t *wr_iothread;
} ovs_async_threads;

struct _ovs_completion_t
{
    ovs_callback_t complete_cb;
    void *cb_arg;
    /* Internal members */
    bool _on_wait;
    bool _calling;
    bool _signaled;
    ssize_t _rv;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
};

struct _ovs_aio_request
{
    struct ovs_aiocb *ovs_aiocbp;
    ovs_completion_t *completion;
};

#endif
