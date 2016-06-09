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

#ifndef __LIB_OVS_INTERNAL_H
#define __LIB_OVS_INTERNAL_H

#include "common.h"

struct ovs_aio_request
{
    struct ovs_aiocb *ovs_aiocbp;
    ovs_completion_t *_completion;
    RequestOp _op;
    bool _on_suspend;
    bool _canceled;
    bool _completed;
    bool _signaled;
    bool _failed;
    int _errno;
    ssize_t _rv;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;

    ovs_aio_request(RequestOp op,
                    struct ovs_aiocb *aio,
                    ovs_completion_t* comp)
    : ovs_aiocbp(aio)
    , _completion(comp)
    , _op(op)
    {
        /*cnanakos TODO: err handling */
        pthread_cond_init(&_cond, NULL);
        pthread_mutex_init(&_mutex, NULL);
        _on_suspend = false;
        _canceled = false;
        _completed = false;
        _signaled = false;
        _rv = 0;
        if (aio and op != RequestOp::Noop)
        {
            aio->request_ = this;
        }
    }

    ~ovs_aio_request()
    {
        pthread_cond_destroy(&_cond);
        pthread_mutex_destroy(&_mutex);
    }

    void
    try_wake_up_suspended_aiocb()
    {
        if (not __sync_bool_compare_and_swap(&_on_suspend,
                                             false,
                                             true,
                                             __ATOMIC_RELAXED))
        {
            pthread_mutex_lock(&_mutex);
            _signaled = true;
            pthread_cond_signal(&_cond);
            pthread_mutex_unlock(&_mutex);
        }
    }

    void
    shm_complete(int errval,
                 size_t ret,
                 bool failed)
    {
        _errno = errval;
        _rv = ret;
        _failed = failed;
        _completed = true;
        if (_op != RequestOp::AsyncFlush)
        {
            try_wake_up_suspended_aiocb();
        }
        if (_completion)
        {
            _completion->_rv = ret;
            _completion->_failed = _failed;
        }
    }

    void
    xio_complete(ssize_t retval,
                 int errval)
    {
        _errno = errval;
        _rv = retval;
        _failed = (retval == -1 ? true : false);
        _completed = true;
        if (_op != RequestOp::AsyncFlush)
        {
            try_wake_up_suspended_aiocb();
        }
        if (_completion)
        {
            _completion->_rv = retval;
            _completion->_failed = _failed;
        }
    }

    bool
    is_async()
    {
        if (_op == RequestOp::AsyncFlush)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    ovs_completion_t*
    get_completion()
    {
        return _completion;
    }

    struct ovs_aiocb*
    get_aio()
    {
        return ovs_aiocbp;
    }
};

#endif //__LIB_OVS_INTERNAL_H
