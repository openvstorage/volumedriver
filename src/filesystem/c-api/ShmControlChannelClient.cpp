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

#include "ShmControlChannelClient.h"

#include <string>

#include <boost/asio.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/thread/lock_guard.hpp>

#include <youtils/SpinLock.h>

namespace ipc = boost::interprocess;
namespace asio = boost::asio;

ShmControlChannelClient::ShmControlChannelClient()
    : socket_(io_service_)
    , ep_(ShmConnectionDetails::Endpoint())
{
}

ShmControlChannelClient::~ShmControlChannelClient()
{
    socket_.shutdown(asio::local::stream_protocol::socket::shutdown_both);
    socket_.close();
}

bool
ShmControlChannelClient::connect_and_register(const std::string& volume_name,
                                              const std::string& key)
{
    boost::system::error_code ec;

    ShmControlChannelMsg msg(ShmMsgOpcode::Register);
    msg.volume_name(volume_name);
    msg.key(key);

    socket_.connect(ep_, ec);
    if (ec)
    {
        return false;
    }

    if (not _ctl_sendmsg(msg))
    {
        return false;
    }
    if (not _ctl_recvmsg(msg))
    {
        return false;
    }
    if (msg.is_success())
    {
        return true;
    }
    return false;
}

bool
ShmControlChannelClient::deregister()
{
    ShmControlChannelMsg msg(ShmMsgOpcode::Deregister);

    boost::lock_guard<decltype(ctl_channel_lock_)> g(ctl_channel_lock_);
    if (not _ctl_sendmsg(msg))
    {
        return false;
    }
    if (not _ctl_recvmsg(msg))
    {
        return false;
    }
    if (msg.is_success())
    {
        return true;
    }
    return false;
}

bool
ShmControlChannelClient::allocate(ipc::managed_shared_memory::handle_t& hdl,
                                  size_t size)
{
    ShmControlChannelMsg msg(ShmMsgOpcode::Allocate);
    msg.size(size);
    {
        boost::lock_guard<decltype(ctl_channel_lock_)> g(ctl_channel_lock_);
        if (not _ctl_sendmsg(msg))
        {
            return false;
        }
        if (not _ctl_recvmsg(msg))
        {
            return false;
        }
    }
    if (msg.is_success())
    {
        hdl = msg.handle();
        return true;
    }
    return false;
}

bool
ShmControlChannelClient::deallocate(const ipc::managed_shared_memory::handle_t& hdl)
{
    ShmControlChannelMsg msg(ShmMsgOpcode::Deallocate);
    msg.handle(hdl);
    {
        boost::lock_guard<decltype(ctl_channel_lock_)> g(ctl_channel_lock_);
        if (not _ctl_sendmsg(msg))
        {
            return false;
        }
        if (not _ctl_recvmsg(msg))
        {
            return false;
        }
    }
    if (msg.is_success())
    {
        return true;
    }
    return false;
}

bool
ShmControlChannelClient::is_open() const
{
    return socket_.is_open();
}

bool
ShmControlChannelClient::is_connected()
{
    char buf;
    ssize_t ret = recv(socket_.native_handle(),
                       &buf,
                       1,
                       MSG_DONTWAIT | MSG_PEEK);
    if (ret == 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}
