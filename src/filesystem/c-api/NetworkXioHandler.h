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

#ifndef __NETWORK_XIO_HANDLER_H
#define __NETWORK_XIO_HANDLER_H

#include "common.h"
#include "AioCompletion.h"

namespace volumedriverfs
{

static void
_xio_aio_wake_up_suspended_aiocb(ovs_aio_request *request)
{
    if (not __sync_bool_compare_and_swap(&request->_on_suspend,
                                         false,
                                         true,
                                         __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&request->_mutex);
        request->_signaled = true;
        pthread_cond_signal(&request->_cond);
        pthread_mutex_unlock(&request->_mutex);
    }
}

void
ovs_xio_aio_complete_request(void* opaque, ssize_t retval, int errval)
{
    ovs_aio_request *request = reinterpret_cast<ovs_aio_request*>(opaque);
    ovs_completion_t *completion = request->completion;
    RequestOp op = request->_op;
    request->_errno = errval;
    request->_rv = retval;
    request->_failed = (retval == -1 ? true : false);
    request->_completed = true;
    if (op != RequestOp::AsyncFlush)
    {
        _xio_aio_wake_up_suspended_aiocb(request);
    }
    if (completion)
    {
        completion->_rv = retval;
        completion->_failed = (retval == -1 ? true : false);
        if (RequestOp::AsyncFlush == op)
        {
            pthread_mutex_destroy(&request->_mutex);
            pthread_cond_destroy(&request->_cond);
            delete request->ovs_aiocbp;
            delete request;
        }
        AioCompletion::get_aio_context().schedule(completion);
    }
}

}
#endif //__NETWORK_XIO_HANDLER_H
