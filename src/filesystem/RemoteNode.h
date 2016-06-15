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

#ifndef VFS_REMOTE_NODE_H_
#define VFS_REMOTE_NODE_H_

#include "ClusterNode.h"
#include "ClusterNodeConfig.h"
#include "Messages.pb.h"
#include "NodeId.h"

#include <memory>
#include <mutex>

#include <cppzmq/zmq.hpp>

#include <youtils/Logging.h>

namespace volumedriverfs
{

class RemoteNode final
    : public ClusterNode
{
public:
    RemoteNode(ObjectRouter& vrouter,
               const ClusterNodeConfig& cfg,
               std::shared_ptr<zmq::context_t> ztx);

    RemoteNode(const RemoteNode&) = delete;

    RemoteNode&
    operator=(const RemoteNode&) = delete;

    virtual void
    read(const Object& obj,
         uint8_t* buf,
         size_t* size,
         off_t off) override final;

    virtual void
    write(const Object& obj,
          const uint8_t* buf,
          size_t* size,
          off_t off) override final;

    virtual void
    sync(const Object& obj) override final;

    virtual uint64_t
    get_size(const Object& obj) override final;

    virtual void
    resize(const Object& obj,
           uint64_t newsize) override final;

    virtual void
    unlink(const Object& id) override final;

    void
    transfer(const Object& obj);

    void
    ping();

private:
    DECLARE_LOGGER("VFSRemoteNode");

    std::shared_ptr<zmq::context_t> ztx_;
    std::unique_ptr<zmq::socket_t> zock_;

    // Protects the zock_ as it's accessed from different (fuse)
    // threads concurrently.
    // Not exactly the recommended way of using ZMQ (which would be
    // a socket per thread).
    typedef std::mutex lock_type;
    mutable lock_type lock_;

    void
    init_zock_();

    void
    wait_for_remote_(const boost::chrono::milliseconds& timeout_ms);

    typedef std::function<void()> ExtraSendFun;
    typedef std::function<void()> ExtraRecvFun;

    template<typename Request>
    void
    handle_(const Request& req,
            const boost::chrono::milliseconds& timeout_ms,
            ExtraSendFun* send_extra = nullptr,
            ExtraRecvFun* recv_extra = nullptr);
};

}

#endif // !VFS_REMOTE_NODE_H_
