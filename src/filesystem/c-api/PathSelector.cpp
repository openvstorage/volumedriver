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

#include "Logger.h"
#include "PathSelector.h"

#include <chrono>

#include <boost/optional/optional_io.hpp>

#include <youtils/IOException.h>

namespace libovsvolumedriver
{

#define LOCK_CTX()                              \
    std::lock_guard<decltype(ctx_lock_)> clg__(ctx_lock_)

#define LOCK_MAP()                              \
    std::lock_guard<decltype(map_lock_)> mlg__(map_lock_)

PathSelector::PathSelector(const std::string& uri,
                           ContextFactoryFun make_context,
                           RequestDispatcherCallback& callback)
    : ctx_(make_context(uri, *this))
    , make_context_(std::move(make_context))
    , callback_(callback)
    , period_secs_(30)
    , checker_("PathChecker",
               [&]
               {
                   update_();
               },
               period_secs_)
{}

void
PathSelector::update_()
{
    const boost::optional<std::string> vname(volume_name());
    if (vname)
    {
        ContextPtr ctx = get_ctx_();
        std::string uri;
        int ret = ctx->get_volume_uri(vname->c_str(),
                                      uri);
        if (ret)
        {
            LIBLOG_ERROR(*vname << ": failed to get volume URI");
            throw fungi::IOException("failed to get volume URI");
        }

        const std::string cur_uri(current_uri());
        if (uri != cur_uri)
        {
            LIBLOG_INFO(*vname << ": URI has changed from " << cur_uri << " to " << uri <<
                        " - attempting to follow");

            ContextPtr ctx(make_context_(uri,
                                         *this));

            LOCK_CTX();
            std::swap(ctx, ctx_);
        }
    }
}

template<typename... Args>
int
PathSelector::wrap_io_(int (ovs_context_t::*mem_fun)(ovs_aio_request*,
                                                     Args...),
                       ovs_aio_request* req,
                       Args... args)
{
    ContextPtr ctx(get_ctx_());
    auto key = reinterpret_cast<uint64_t>(req);

    {
        LOCK_MAP();
        auto ret(map_.emplace(key, ctx));
    }

    int ret = (ctx.get()->*mem_fun)(req,
                                    std::forward<Args>(args)...);
    if (ret < 0)
    {
        map_.erase(key);
    }

    return ret;
}

int
PathSelector::send_read_request(ovs_aio_request* req,
                                ovs_aiocb* aiocb)
{
    return wrap_io_(&ovs_context_t::send_read_request,
                    req,
                    aiocb);
}

int
PathSelector::send_write_request(ovs_aio_request* req,
                                 ovs_aiocb* aiocb)
{
    return wrap_io_(&ovs_context_t::send_write_request,
                    req,
                    aiocb);
}

int
PathSelector::send_flush_request(ovs_aio_request* req)
{
    return wrap_io_(&ovs_context_t::send_flush_request,
                    req);
}

void
PathSelector::complete_request(ovs_aio_request& req,
                               ssize_t ret,
                               int err,
                               bool schedule)
{
    {
        LOCK_MAP();
        map_.erase(reinterpret_cast<uint64_t>(&req));
    }

    callback_.complete_request(req, ret, err, schedule);
}

}
