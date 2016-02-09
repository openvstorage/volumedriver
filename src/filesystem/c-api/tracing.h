// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __LIB_OVS_TRACING_H
#define __LIB_OVS_TRACING_H

#include "TracePoints_tp.h"

#define safe_errno_tracepoint(...) \
    do { \
        int saved_errno = errno; \
        tracepoint(__VA_ARGS__); \
        errno = saved_errno; \
    } while(0)

static inline
void ovs_submit_aio_request_tracepoint_enter(const RequestOp& op,
                                             const ovs_ctx_t* ctx,
                                             const struct ovs_aiocb *aiocbp,
                                             const ovs_completion_t *completion)
{
    switch (op)
    {
    case RequestOp::Read:
        if (completion)
        {
            tracepoint(openvstorage_libovsvolumedriver,
                       ovs_aio_readcb_enter,
                       ctx,
                       aiocbp,
                       completion,
                       aiocbp ? aiocbp->aio_nbytes : 0,
                       aiocbp ? aiocbp->aio_offset : 0);

        }
        else
        {
            tracepoint(openvstorage_libovsvolumedriver,
                       ovs_aio_read_enter,
                       ctx,
                       aiocbp,
                       aiocbp ? aiocbp->aio_nbytes : 0,
                       aiocbp ? aiocbp->aio_offset : 0);
        }
        break;
    case RequestOp::Write:
        if (completion)
        {
            tracepoint(openvstorage_libovsvolumedriver,
                       ovs_aio_writecb_enter,
                       ctx,
                       aiocbp,
                       completion,
                       aiocbp ? aiocbp->aio_nbytes : 0,
                       aiocbp ? aiocbp->aio_offset : 0);
        }
        else
        {
            tracepoint(openvstorage_libovsvolumedriver,
                       ovs_aio_write_enter,
                       ctx,
                       aiocbp,
                       aiocbp ? aiocbp->aio_nbytes : 0,
                       aiocbp ? aiocbp->aio_offset : 0);
        }
        break;
    default:
        return;
    }
}


static inline
void ovs_submit_aio_request_tracepoint_exit(const RequestOp& op,
                                            const ovs_ctx_t* ctx,
                                            const struct ovs_aiocb *aiocbp,
                                            const ovs_completion_t *completion,
                                            int r,
                                            int saved_errno)
{
    switch (op)
    {
    case RequestOp::Read:
        if (completion)
        {
            tracepoint(openvstorage_libovsvolumedriver,
                       ovs_aio_readcb_exit,
                       ctx,
                       aiocbp,
                       completion,
                       r,
                       saved_errno);
        }
        else
        {
            tracepoint(openvstorage_libovsvolumedriver,
                       ovs_aio_read_exit,
                       ctx,
                       aiocbp,
                       r,
                       saved_errno);
        }
        break;
    case RequestOp::Write:
        if (completion)
        {
            tracepoint(openvstorage_libovsvolumedriver,
                       ovs_aio_writecb_exit,
                       ctx,
                       aiocbp,
                       completion,
                       r,
                       saved_errno);
        }
        else
        {
            tracepoint(openvstorage_libovsvolumedriver,
                       ovs_aio_write_exit,
                       ctx,
                       aiocbp,
                       r,
                       saved_errno);
        }
        break;
    case RequestOp::AsyncFlush:
        tracepoint(openvstorage_libovsvolumedriver,
                   ovs_aio_flushcb_exit,
                   ctx,
                   r,
                   saved_errno);
        break;
    default:
        return;
    }
}

#endif //__LIB_OVS_COMMON_H
