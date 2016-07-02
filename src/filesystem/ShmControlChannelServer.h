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

#ifndef __SHM_CONTROL_CHANNEL_SERVER_H
#define __SHM_CONTROL_CHANNEL_SERVER_H

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <youtils/SpinLock.h>
#include <youtils/Assert.h>

#include "ShmControlChannelProtocol.h"
#include "ShmVolumeCache.h"

namespace volumedriverfs
{

typedef boost::function<bool(const std::string&)> TryStopVolume;
typedef boost::function<bool(const std::string&,
                             const std::string&)> IsVolumeValid;

class ControlSession
    : public boost::enable_shared_from_this<ControlSession>
{
public:
    ControlSession(boost::asio::io_service& io_service,
                   const ShmSegmentDetails& segment_details,
                   TryStopVolume try_stop_volume,
                   IsVolumeValid is_volume_valid)
    : socket_(io_service)
    , segment_details_(segment_details)
    , state_(ShmConnectionState::Connected)
    , try_stop_volume_(try_stop_volume)
    , is_volume_valid_(is_volume_valid)
    , deregistered(false)
    {
        data_.resize(1024);
    }

    boost::asio::local::stream_protocol::socket& socket()
    {
        return socket_;
    }

    void
    check_buffer_capacity(const unsigned int& size)
    {
        if (data_.size() < size)
        {
            data_.resize(size);
        }
    }

    void
    remove_session()
    {
        if (not deregistered)
        {
            try_stop_volume_(volume_name_);
            cache_.reset();
        }
    }

    ShmControlChannelMsg
    handle_register(const ShmControlChannelMsg& msg)
    {
        ShmControlChannelMsg o_msg(ShmMsgOpcode::Failed);
        if (state_ == ShmConnectionState::Connected)
        {
            if (is_volume_valid_(msg.volume_name(), msg.key()))
            {
                volume_name_ = msg.volume_name();
                cache_.reset(new ShmVolumeCache(segment_details_));
                state_ = ShmConnectionState::Registered;
                o_msg.opcode(ShmMsgOpcode::Success);
            }
        }
        return o_msg;
    }

    ShmControlChannelMsg
    handle_allocate(const ShmControlChannelMsg& msg)
    {
        ShmControlChannelMsg o_msg(ShmMsgOpcode::Failed);
        if (state_ == ShmConnectionState::Registered)
        {
            try
            {
                o_msg.handle(cache_->allocate(msg.size()));
                o_msg.opcode(ShmMsgOpcode::Success);
            }
            catch (boost::interprocess::bad_alloc&)
            {
                o_msg.opcode(ShmMsgOpcode::Failed);
            }
        }
        return o_msg;
    }

    ShmControlChannelMsg
    handle_deallocate(const ShmControlChannelMsg& msg)
    {
        ShmControlChannelMsg o_msg(ShmMsgOpcode::Failed);
        if (state_ == ShmConnectionState::Registered)
        {
            //cnanakos: make deallocation async and return immediately
            bool ret = cache_->deallocate(msg.handle());
            if (ret)
            {
                o_msg.opcode(ShmMsgOpcode::Success);
            }
        }
        return o_msg;
    }

    ShmControlChannelMsg
    handle_unregister()
    {
        ShmControlChannelMsg o_msg(ShmMsgOpcode::Failed);
        if (state_ == ShmConnectionState::Registered)
        {
            cache_.reset();
            state_ = ShmConnectionState::Connected;
            o_msg.opcode(ShmMsgOpcode::Success);
            deregistered = true;
        }
        return o_msg;
    }

    void async_start()
    {
        boost::asio::async_read(socket_,
                                boost::asio::buffer(&payload_size_,
                                                    shm_msg_header_size),
                                boost::bind(&ControlSession::handle_header_read,
                                            shared_from_this(),
                                            boost::asio::placeholders::error));
    }

    void
    handle_header_read(const boost::system::error_code& error)
    {
        if (not error)
        {
            check_buffer_capacity(payload_size_);
            boost::asio::async_read(socket_,
                                    boost::asio::buffer(data_,
                                                        payload_size_),
                                    boost::bind(&ControlSession::handle_read,
                                                shared_from_this(),
                                                boost::asio::placeholders::error,
                                                boost::asio::placeholders::bytes_transferred));
        }
        else if (error == boost::asio::error::eof ||
                 error == boost::asio::error::connection_reset)
        {
            remove_session();
        }
    }

    ShmControlChannelMsg
    handle_state(const char *data,
                 const size_t size)
    {
        ShmControlChannelMsg i_msg(ShmMsgOpcode::Failed);
        i_msg.unpack_msg(data, size);
        switch (i_msg.opcode())
        {
        case ShmMsgOpcode::Register:
            return handle_register(i_msg);
        case ShmMsgOpcode::Deregister:
            return handle_unregister();
        case ShmMsgOpcode::Allocate:
            return handle_allocate(i_msg);
        case ShmMsgOpcode::Deallocate:
            return handle_deallocate(i_msg);
        default:
            return ShmControlChannelMsg(ShmMsgOpcode::Failed);
        }
    }

    void
    handle_read(const boost::system::error_code& error,
                const size_t& bytes_transferred)
    {
        if (not error)
        {
            VERIFY(payload_size_ == bytes_transferred);
            ShmControlChannelMsg msg(handle_state(data_.data(),
                                                  bytes_transferred));
            std::string msg_str(msg.pack_msg());
            uint64_t reply_payload_size = msg_str.length();

            boost::asio::async_write(socket_,
                                     boost::asio::buffer(&reply_payload_size,
                                                         shm_msg_header_size),
                                     boost::bind(&ControlSession::handle_header_write,
                                                 shared_from_this(),
                                                 msg_str,
                                                 reply_payload_size,
                                                 boost::asio::placeholders::error));
        }
        else if (error == boost::asio::error::eof ||
                 error == boost::asio::error::connection_reset)
        {
            remove_session();
        }
    }

    void
    handle_header_write(const std::string& msg,
                        const uint64_t& payload_size,
                        const boost::system::error_code& error)
    {
        if (not error)
        {
            boost::asio::async_write(socket_,
                                     boost::asio::buffer(msg,
                                                         payload_size),
                                     boost::bind(&ControlSession::handle_write,
                                                 shared_from_this(),
                                                 boost::asio::placeholders::error));
        }
        else if (error == boost::asio::error::eof ||
                 error == boost::asio::error::connection_reset)
        {
            remove_session();
        }
    }

    void
    handle_write(const boost::system::error_code& error)
    {
        if (not error)
        {
            async_start();
        }
        else if (error == boost::asio::error::eof ||
                 error == boost::asio::error::connection_reset)
        {
            remove_session();
        }
    }

private:
    DECLARE_LOGGER("ShmControlSession");

    // TODO:
    // * constify where possible
    // * pointer back to the ShmControlChannelServer instead of duplicating the ShmSegmentDetails
    boost::asio::local::stream_protocol::socket socket_;
    std::vector<char> data_;
    uint64_t payload_size_;
    ShmVolumeCachePtr cache_;
    ShmSegmentDetails segment_details_;
    std::string volume_name_;
    ShmConnectionState state_;
    TryStopVolume try_stop_volume_;
    IsVolumeValid is_volume_valid_;
    bool deregistered;
};

typedef boost::shared_ptr<ControlSession> ctl_session_ptr;

class ShmControlChannelServer
{
public:
    ShmControlChannelServer(const ShmSegmentDetails& segment_details)
        : acceptor_(io_service_)
        , segment_details_(segment_details)
    {
        boost::filesystem::remove_all(segment_details_.control_endpoint());
        boost::asio::local::stream_protocol::endpoint ep(segment_details_.control_endpoint());
        acceptor_.open(ep.protocol());
        acceptor_.bind(ep);
        acceptor_.listen(5);
    }

    void
    handle_accept(ctl_session_ptr new_session,
                  const boost::system::error_code& error)
    {
        if (not error)
        {
            new_session->async_start();
        }
        new_session.reset(new ControlSession(io_service_,
                                             segment_details_,
                                             try_stop_volume_,
                                             is_volume_valid_));
        acceptor_.async_accept(new_session->socket(),
                                boost::bind(&ShmControlChannelServer::handle_accept,
                                            this,
                                            new_session,
                                            boost::asio::placeholders::error));
    }

    // TODO: move into ctor
    void
    create_control_channel(TryStopVolume try_stop_volume,
                           IsVolumeValid is_volume_valid)
    {

        ctl_session_ptr new_session(new ControlSession(io_service_,
                                                       segment_details_,
                                                       try_stop_volume,
                                                       is_volume_valid));
        acceptor_.async_accept(new_session->socket(),
                               boost::bind(&ShmControlChannelServer::handle_accept,
                                           this,
                                           new_session,
                                           boost::asio::placeholders::error));
        try_stop_volume_ = try_stop_volume;
        is_volume_valid_ = is_volume_valid;
        grp_.create_thread(boost::bind(&boost::asio::io_service::run,
                                       &io_service_));
    }

    // TODO: move into dtor
    void
    destroy_control_channel()
    {
        io_service_.stop();
        grp_.join_all();
        boost::filesystem::remove_all(segment_details_.control_endpoint());
    }

    const ShmSegmentDetails&
    shm_segment_details() const
    {
        return segment_details_;
    }

private:
    boost::asio::io_service io_service_;
    boost::asio::local::stream_protocol::acceptor acceptor_;
    boost::thread_group grp_;
    ShmSegmentDetails segment_details_;
    TryStopVolume try_stop_volume_;
    IsVolumeValid is_volume_valid_;
};

} //namespace volumedriverfs

#endif //__SHM_CONTROL_CHANNEL_SERVER_H
