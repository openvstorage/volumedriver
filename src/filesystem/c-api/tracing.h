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
