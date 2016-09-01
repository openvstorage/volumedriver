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

#ifndef __SHM_CONTROL_CHANNEL_CLIENT_H
#define __SHM_CONTROL_CHANNEL_CLIENT_H

#include "../ShmControlChannelProtocol.h"

#include <boost/asio.hpp>
#include <youtils/SpinLock.h>

#include <memory>

class ShmControlChannelClient
{
public:
    ShmControlChannelClient();

    ~ShmControlChannelClient();

    ShmControlChannelClient(const ShmControlChannelClient&) = delete;
    ShmControlChannelClient& operator=(const ShmControlChannelClient&) = delete;

    bool
    connect_and_register(const std::string& volume_name,
                         const std::string& key);

    bool
    deregister();

    bool
    allocate(boost::interprocess::managed_shared_memory::handle_t& hdl,
             size_t size);

    bool
    deallocate(const boost::interprocess::managed_shared_memory::handle_t& hdl);

    bool
    is_open() const;

    bool
    is_connected();

private:
    boost::asio::io_service io_service_;
    boost::asio::local::stream_protocol::socket socket_;
    boost::asio::local::stream_protocol::endpoint ep_;
    fungi::SpinLock ctl_channel_lock_;

    bool
    _ctl_sendmsg(const ShmControlChannelMsg& msg)
    {
        const std::string msg_str(msg.pack_msg());
        uint64_t msg_payload = msg_str.length();

        try
        {
            boost::asio::write(socket_,
                               boost::asio::buffer(&msg_payload,
                                                   shm_msg_header_size));
            boost::asio::write(socket_,
                               boost::asio::buffer(msg_str,
                                                   msg_payload));
        }
        catch(boost::system::system_error&)
        {
            return false;
        }
        return true;
    }

    bool
    _ctl_recvmsg(ShmControlChannelMsg& msg)
    {
        uint64_t payload_size;
        try
        {
            std::vector<char> reply;
            std::size_t len = boost::asio::read(socket_,
                                                boost::asio::buffer(&payload_size,
                                                                    shm_msg_header_size));
            assert(len == shm_msg_header_size);
            reply.resize(payload_size);
            len = boost::asio::read(socket_,
                                    boost::asio::buffer(reply,
                                                        payload_size));
            assert(len == payload_size);
            msg.unpack_msg(reply.data(),
                           len);
        }
        catch(boost::system::system_error&)
        {
            return false;
        }
        return true;
    }
};

typedef std::shared_ptr<ShmControlChannelClient> ShmControlChannelClientPtr;

#endif //__SHM_CONTROL_CHANNEL_CLIENT_H
