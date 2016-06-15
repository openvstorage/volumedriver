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

#ifndef __NETWORK_XIO_HANDLER_H
#define __NETWORK_XIO_HANDLER_H

#include "internal.h"
#include "AioCompletion.h"

namespace volumedriverfs
{

void
ovs_xio_aio_complete_request(void* opaque, ssize_t retval, int errval)
{
    ovs_aio_request *request = reinterpret_cast<ovs_aio_request*>(opaque);
    ovs_completion_t *completion = request->get_completion();
    struct ovs_aiocb *aiocbp = request->get_aio();
    request->xio_complete(retval,
                          errval);
    if (completion)
    {
        request->set_completion();
        if (request->is_async_flush())
        {
            delete aiocbp;
            delete request;
        }
        AioCompletion::get_aio_context().schedule(completion);
    }
}

void
ovs_xio_complete_request_control(void *opaque, ssize_t retval, int errval)
{
    ovs_aio_request *request = reinterpret_cast<ovs_aio_request*>(opaque);
    if (request)
    {
        request->_errno = errval;
        request->_rv = retval;
    }
}

}
#endif //__NETWORK_XIO_HANDLER_H
