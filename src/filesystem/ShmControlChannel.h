// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef __SHM_CONTROL_CHANNEL_H
#define __SHM_CONTROL_CHANNEL_H

#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include <youtils/SpinLock.h>
#include <youtils/Assert.h>

#include "ShmControlChannelProtocol.h"

typedef boost::function<bool(const std::string&)> TryStopVolume;
typedef boost::function<bool(const std::string&,
                             const std::string&)> IsVolumeValid;

class ControlSession
    : public boost::enable_shared_from_this<ControlSession>
{
public:
    ControlSession(boost::asio::io_service& io_service,
                   std::map<int, std::string>& sess_,
                   fungi::SpinLock& lock_,
                   TryStopVolume try_stop_volume,
                   IsVolumeValid is_volume_valid)
    : socket_(io_service)
    , sessions_(sess_)
    , sessions_lock_(lock_)
    , state_(ShmConnectionState::Connected)
    , try_stop_volume_(try_stop_volume)
    , is_volume_valid_(is_volume_valid)
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
        fungi::ScopedSpinLock lock_(sessions_lock_);
        auto it = sessions_.find(socket_.native());
        if (it != sessions_.end())
        {
            try_stop_volume_(it->second);
            sessions_.erase(it);
        }
    }

    ShmControlChannelMsg
    handle_register(const ShmControlChannelMsg& msg)
    {
        ShmControlChannelMsg o_msg(ShmMsgOpcode::Failed);
        if (state_ == ShmConnectionState::Connected)
        {
            fungi::ScopedSpinLock lock_(sessions_lock_);
            if (is_volume_valid_(msg.volname_, msg.key_))
            {
                sessions_.emplace(socket_.native(),
                                  msg.volname_);
                state_ = ShmConnectionState::Registered;
                o_msg.opcode_ = ShmMsgOpcode::Success;
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
            fungi::ScopedSpinLock lock_(sessions_lock_);
            auto it = sessions_.find(socket_.native());
            if (it != sessions_.end())
            {
                sessions_.erase(it);
                state_ = ShmConnectionState::Connected;
                o_msg.opcode_ = ShmMsgOpcode::Success;
            }
        }
        return o_msg;
    }

    void async_start()
    {
        boost::asio::async_read(socket_,
                                boost::asio::buffer(&payload_size_,
                                                    header_size),
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
        ShmControlChannelMsg i_msg;
        i_msg.unpack_msg(data, size);
        switch (i_msg.opcode_)
        {
        case ShmMsgOpcode::Register:
            return handle_register(i_msg);
        case ShmMsgOpcode::Deregister:
            return handle_unregister();
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
                                                         header_size),
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

    boost::asio::local::stream_protocol::socket socket_;
    std::vector<char> data_;
    uint64_t payload_size_;
    std::map<int, std::string>& sessions_;
    fungi::SpinLock& sessions_lock_;
    ShmConnectionState state_;
    TryStopVolume try_stop_volume_;
    IsVolumeValid is_volume_valid_;
};

typedef boost::shared_ptr<ControlSession> ctl_session_ptr;

class ShmControlChannel
{
public:
    ShmControlChannel(const std::string& file)
        : acceptor_(io_service_)
        , file_(file)
    {
        ::unlink(file.c_str());
        boost::asio::local::stream_protocol::endpoint ep(file);
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
                                             sessions_,
                                             sessions_lock_,
                                             try_stop_volume_,
                                             is_volume_valid_));
        acceptor_.async_accept(new_session->socket(),
                                boost::bind(&ShmControlChannel::handle_accept,
                                            this,
                                            new_session,
                                            boost::asio::placeholders::error));
    }

    void
    create_control_channel(TryStopVolume try_stop_volume,
                           IsVolumeValid is_volume_valid)
    {

        ctl_session_ptr new_session(new ControlSession(io_service_,
                                                       sessions_,
                                                       sessions_lock_,
                                                       try_stop_volume,
                                                       is_volume_valid));
        acceptor_.async_accept(new_session->socket(),
                               boost::bind(&ShmControlChannel::handle_accept,
                                           this,
                                           new_session,
                                           boost::asio::placeholders::error));
        try_stop_volume_ = try_stop_volume;
        is_volume_valid_ = is_volume_valid;
        grp_.create_thread(boost::bind(&boost::asio::io_service::run,
                                       &io_service_));
    }

    void
    destroy_control_channel()
    {
        io_service_.stop();
        grp_.join_all();
        ::unlink(file_.c_str());
    }

private:
    boost::asio::io_service io_service_;
    boost::asio::local::stream_protocol::acceptor acceptor_;
    std::map<int, std::string> sessions_;
    fungi::SpinLock sessions_lock_;
    boost::thread_group grp_;
    std::string file_;
    TryStopVolume try_stop_volume_;
    IsVolumeValid is_volume_valid_;
};

#endif //__SHM_CONTROL_CHANNEL_H
