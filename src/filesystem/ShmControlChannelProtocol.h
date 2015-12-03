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

#ifndef __SHM_CONTROL_CHANNEL_PROTOCOL_H_
#define __SHM_CONTROL_CHANNEL_PROTOCOL_H_

#include <boost/interprocess/managed_shared_memory.hpp>
#include <msgpack.hpp>

enum class ShmConnectionState
{
    Idle,
    Registering,
    Registered,
    Connecting,
    Connected,
    Disconnecting,
    Disconnected,
};

struct ShmConnectionDetails
{
    static const char* Endpoint()
    {
        return "/tmp/ovs-shm-ctl.socket";
    }
};

enum class ShmMsgOpcode
{
    Idle,
    Failed,
    Success,
    Register,
    Deregister,
    Allocate,
    Deallocate,
};

class ShmControlChannelMsg
{
public:
    ShmControlChannelMsg(ShmMsgOpcode opcode,
                         const boost::interprocess::managed_shared_memory::handle_t& hdl =
                            boost::interprocess::managed_shared_memory::handle_t(0),
                         const std::string& volname = "",
                         const std::string& key = "",
                         const long& opaque = 0)
    : opcode_(opcode)
    , volname_(volname)
    , key_(key)
    , opaque_(opaque)
    , hdl_(hdl)
    , size_(0)
    {}

    ShmControlChannelMsg()
    {}

public:
    ShmMsgOpcode opcode_;
    std::string volname_;
    std::string key_;
    long opaque_;
    boost::interprocess::managed_shared_memory::handle_t hdl_;
    size_t size_;

public:
    const ShmMsgOpcode&
    opcode() const
    {
        return opcode_;
    }

    void
    opcode(const ShmMsgOpcode& op)
    {
        opcode_ = op;
    }

    const std::string&
    volume_name() const
    {
        return volname_;
    }

    void
    volume_name(const std::string& volume)
    {
        volname_ = volume;
    }

    const std::string&
    key() const
    {
        return key_;
    }

    void
    key(const std::string& key)
    {
        key_ = key;
    }

    const long&
    opaque() const
    {
        return opaque_;
    }

    void
    opaque(const long& opq)
    {
        opaque_ = opq;
    }

    const std::string
    pack_msg() const
    {
        std::stringstream sbuf;
        msgpack::pack(sbuf, *this);
        return sbuf.str();
    }

    void
    handle(const boost::interprocess::managed_shared_memory::handle_t& hdl)
    {
        hdl_ = hdl;
    }

    const boost::interprocess::managed_shared_memory::handle_t&
    handle() const
    {
        return hdl_;
    }

    const size_t&
    size() const
    {
        return size_;
    }

    void
    size(const size_t& size)
    {
        size_ = size;
    }

    void
    unpack_msg(const char *sbuf,
               const size_t size)
    {
        msgpack::unpacked msg;
        msgpack::unpack(&msg, sbuf, size);
        msgpack::object obj = msg.get();
        obj.convert(this);
    }

    void
    unpack_msg(const std::string& sbuf)
    {
        msgpack::unpacked msg;
        msgpack::unpack(&msg, sbuf.data(), sbuf.size());
        msgpack::object obj = msg.get();
        obj.convert(this);
    }

    bool
    is_success()
    {
        return (opcode() == ShmMsgOpcode::Success);
    }

    void
    clear()
    {
        volname_.clear();
        key_.clear();
        opaque_ = 0;
        hdl_ = 0;
        size_ = 0;
    }
public:
    MSGPACK_DEFINE(opcode_,
                   volname_,
                   key_,
                   opaque_,
                   hdl_,
                   size_);
};

MSGPACK_ADD_ENUM(ShmMsgOpcode);

static const uint64_t shm_msg_header_size = sizeof(uint64_t);

#endif //__SHM_CONTROL_CHANNEL_PROTOCOL_H_
