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

#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include <cppzmq/zmq.hpp>

#include <youtils/Logging.h>

#include <volumedriver/ClusterLocation.h>

namespace volumedriverfstest
{
class FileSystemTestBase;
}

namespace volumedriverfs
{

class RemoteNode final
    : public ClusterNode
{
    friend class volumedriverfstest::FileSystemTestBase;

public:
    RemoteNode(ObjectRouter&,
               const NodeId&,
               const youtils::Uri&,
               zmq::context_t&);

    ~RemoteNode();

    RemoteNode(const RemoteNode&) = delete;

    RemoteNode&
    operator=(const RemoteNode&) = delete;

    void
    read(const Object&,
         uint8_t* buf,
         size_t* size,
         off_t off) final;

    void
    write(const Object&,
          const uint8_t* buf,
          size_t* size,
          off_t off,
          volumedriver::DtlInSync&) final;

    void
    sync(const Object&,
         volumedriver::DtlInSync&) final;

    uint64_t
    get_size(const Object&) final;

    volumedriver::ClusterMultiplier
    get_cluster_multiplier(const Object& obj) final;

    volumedriver::CloneNamespaceMap
    get_clone_namespace_map(const Object& obj) final;

    std::vector<volumedriver::ClusterLocation>
    get_page(const Object& obj,
             const volumedriver::ClusterAddress ca) final;

    void
    resize(const Object&,
           uint64_t newsize) final;

    void
    unlink(const Object&) final;

    void
    open(const Object&) final;

    void
    transfer(const Object&);

    void
    ping();

private:
    DECLARE_LOGGER("VFSRemoteNode");

    zmq::context_t& ztx_;
    std::unique_ptr<zmq::socket_t> zock_;
    int event_fd_;
    int timer_fd_;
    bool stop_;
    boost::thread thread_;

    // Protects the work queue / map.
    boost::mutex work_lock_;

    struct WorkItem;
    using WorkItemPtr = boost::shared_ptr<WorkItem>;

    std::list<WorkItemPtr> queued_work_; // push back / pop front
    std::map<vfsprotocol::Tag, WorkItemPtr> submitted_work_;

    // no locking, only ever accessed from within thread_
    const vfsprotocol::PingMessage keepalive_probe_;
    WorkItemPtr keepalive_work_;
    std::atomic<size_t> missing_keepalive_probes_;

    void
    close_();

    void
    notify_();

    void
    reset_();

    void
    arm_keepalive_timer_(const boost::chrono::seconds&);

    void
    keepalive_();

    boost::chrono::seconds
    check_keepalive_();

    void
    drop_keepalive_work_();

    bool
    send_requests_();

    void
    submit_work_(const WorkItemPtr&);

    void
    drop_request_(const WorkItem&);

    void
    recv_responses_();

    void
    handle_response_(WorkItem&);

    void
    work_();

    typedef std::function<void()> ExtraSendFun;
    typedef std::function<void()> ExtraRecvFun;

    template<typename Request>
    void
    handle_(const Request&,
            const boost::chrono::milliseconds& timeout_ms,
            ExtraSendFun = ExtraSendFun(),
            ExtraRecvFun = ExtraRecvFun());
};

}

#endif // !VFS_REMOTE_NODE_H_
