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
    Allocatev,
    Deallocatev,
};

class ShmControlChannelMsg
{
public:
    ShmControlChannelMsg(ShmMsgOpcode opcode,
                         const std::vector<uint64_t>& handles = std::vector<uint64_t>(),
                         const std::string& volname = "",
                         const std::string& key = "",
                         const long& opaque = 0)
    : opcode_(opcode)
    , handles_(handles)
    , volname_(volname)
    , key_(key)
    , opaque_(opaque)
    {}

    ShmControlChannelMsg()
    {}

public:
    ShmMsgOpcode opcode_;
    std::vector<uint64_t> handles_;
    std::string volname_;
    std::string key_;
    long opaque_;

public:
    const std::string
    pack_msg() const
    {
        std::stringstream sbuf;
        msgpack::pack(sbuf, *this);
        return sbuf.str();
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

    void
    clear()
    {
        handles_.clear();
        volname_.clear();
        key_.clear();
        opaque_ = 0;
    }
public:
    MSGPACK_DEFINE(opcode_,
                   handles_,
                   volname_,
                   key_,
                   opaque_);
};

MSGPACK_ADD_ENUM(ShmMsgOpcode);

static const uint64_t header_size = sizeof(uint64_t);

#endif //__SHM_CONTROL_CHANNEL_PROTOCOL_H_
