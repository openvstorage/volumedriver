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
         off_t off) override;

    virtual void
    write(const Object& obj,
          const uint8_t* buf,
          size_t* size,
          off_t off) override;

    virtual void
    sync(const Object& obj) override;

    virtual uint64_t
    get_size(const Object& obj) override;

    virtual void
    resize(const Object& obj,
           uint64_t newsize) override;

    virtual void
    unlink(const Object& id) override;

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
