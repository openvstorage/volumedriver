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

#ifndef __SHM_CONTROL_CHANNEL_PROTOCOL_H_
#define __SHM_CONTROL_CHANNEL_PROTOCOL_H_

#include <boost/interprocess/managed_shared_memory.hpp>
#include <youtils/Assert.h>

PRAGMA_IGNORE_WARNING_BEGIN("-Wctor-dtor-privacy");
#include <msgpack.hpp>
PRAGMA_IGNORE_WARNING_END;

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
    explicit ShmControlChannelMsg(ShmMsgOpcode opcode,
                                  const boost::interprocess::managed_shared_memory::handle_t& hdl =
                                  boost::interprocess::managed_shared_memory::handle_t(0),
                                  const std::string& volname = "",
                                  const std::string& key = "",
                                  const long opaque = 0)
    : opcode_(opcode)
    , volname_(volname)
    , key_(key)
    , opaque_(opaque)
    , hdl_(hdl)
    , size_(0)
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
