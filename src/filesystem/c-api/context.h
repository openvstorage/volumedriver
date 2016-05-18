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

#include <boost/asio.hpp>

#include "../ShmControlChannelProtocol.h"
#include "../ShmClient.h"
#include "../NetworkXioClient.h"

struct ovs_context_t
{
    TransportType transport;
    std::string host;
    int port;
    std::string uri;
    std::string volume_name;
    int oflag;
    ovs_shm_context *shm_ctx_;
    volumedriverfs::NetworkXioClientPtr net_client_;
    uint64_t net_client_qdepth;
};

static bool
_is_volume_name_valid(const char *volume_name)
{
    if (volume_name == NULL || strlen(volume_name) == 0 ||
        strlen(volume_name) >= NAME_MAX)
    {
        return false;
    }
    else
    {
        return true;
    }
}

static inline int
_hostname_to_ip(const char *hostname, std::string& ip)
{

    try
    {
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::resolver resolver(io_service);
        boost::asio::ip::tcp::resolver::query query(std::string(hostname), "");
        for (boost::asio::ip::tcp::resolver::iterator i = resolver.resolve(query);
                i != boost::asio::ip::tcp::resolver::iterator(); i++)
        {
            boost::asio::ip::tcp::endpoint ep = *i;
            ip = ep.address().to_string();
            return 0;
        }
        errno = EINVAL;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

static inline
ovs_aio_request* create_new_request(RequestOp op,
                                    struct ovs_aiocb *aio,
                                    ovs_completion_t *completion)
{
    try
    {
        ovs_aio_request *request = new ovs_aio_request;
        request->_op = op;
        request->ovs_aiocbp = aio;
        request->completion = completion;
        /*cnanakos TODO: err handling */
        pthread_cond_init(&request->_cond, NULL);
        pthread_mutex_init(&request->_mutex, NULL);
        request->_on_suspend = false;
        request->_canceled = false;
        request->_completed = false;
        request->_signaled = false;
        request->_rv = 0;
        if (aio and op != RequestOp::Noop)
        {
            aio->request_ = request;
        }
        return request;
    }
    catch (const std::bad_alloc&)
    {
        return NULL;
    }
}

static int
ovs_xio_open_volume(ovs_ctx_t *ctx, const char *volume_name)
{
    ssize_t r;
    struct ovs_aiocb aio;

    ovs_aio_request *request = create_new_request(RequestOp::Open,
                                                  &aio,
                                                  NULL);
    if (request == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    try
    {
        ctx->net_client_->xio_send_open_request(volume_name,
                                                reinterpret_cast<void*>(request));
    }
    catch (const volumedriverfs::XioClientQueueIsBusyException&)
    {
        errno = EBUSY;  r = -1;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }
    catch (...)
    {
        errno = EIO;
        return -1;
    }

    if ((r = ovs_aio_suspend(ctx, &aio, NULL)) < 0)
    {
        return r;
    }
    r = ovs_aio_return(ctx, &aio);
    if (ovs_aio_finish(ctx, &aio) < 0)
    {
        r = -1;
    }
    return r;
}
#endif // __CONTEXT_H
