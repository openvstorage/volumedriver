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

#include "MessageUtils.h"
#include "ObjectRouter.h"
#include "Protocol.h"
#include "RemoteNode.h"
#include "ZUtils.h"

#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include <boost/chrono.hpp>
#include <boost/exception_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/future.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/ScopeExit.h>
#include <youtils/SourceOfUncertainty.h>

#include <volumedriver/ClusterLocation.h>
// XXX: needed as we throw VolManager::VolumeDoesNotExistException.
// This needs to be redone, we don't want to know about VolManager
// at this level
#include <volumedriver/VolManager.h>

namespace volumedriverfs
{

namespace bc = boost::chrono;
namespace vd = volumedriver;
namespace yt = youtils;

#define LOCK()                                  \
    boost::lock_guard<decltype(work_lock_)> lwg__(work_lock_)

RemoteNode::RemoteNode(ObjectRouter& vrouter,
                       const NodeId& node_id,
                       const yt::Uri& uri,
                       zmq::context_t& ztx)
    : ClusterNode(vrouter, node_id, uri)
    , ztx_(ztx)
    , event_fd_(::eventfd(0,
                          EFD_NONBLOCK))
    , timer_fd_(::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK))
    , stop_(false)
    , keepalive_probe_(vfsprotocol::MessageUtils::create_ping_message(vrouter_.node_id()))
    , missing_keepalive_probes_(0)
{
    auto on_exception(yt::make_scope_exit_on_exception([&]
                                                       {
                                                           close_();
                                                       }));

    VERIFY(event_fd_ >= 0);
    VERIFY(timer_fd_ >= 0);

    reset_();
    thread_ = boost::thread(boost::bind(&RemoteNode::work_,
                                        this));
}

RemoteNode::~RemoteNode()
{
    try
    {
        {
            LOCK();

            drop_keepalive_work_();

            ASSERT(queued_work_.empty());
            ASSERT(submitted_work_.empty());

            stop_ = true;
            notify_();
        }
        thread_.join();
        close_();
    }
    CATCH_STD_ALL_LOG_IGNORE(node_id() << ": exception shutting down");
}

void
RemoteNode::close_()
{
    auto do_close([this](int fd, const char* desc)
                  {
                      if (fd > 0)
                      {
                          int ret = close(fd);
                          if (ret < 0)
                          {
                              LOG_ERROR("Failed to close " <<
                                        desc << " " << fd <<
                                        ": " << strerror(errno));
                          }
                      }
                  });

    do_close(timer_fd_, "timer fd");
    do_close(event_fd_, "event fd");
}

void
RemoteNode::notify_()
{
    int ret;
    const eventfd_t val = 1;
    do {
        ret = eventfd_write(event_fd_,
                            val);
    } while (ret < 0 && errno == EINTR);
    VERIFY(ret == 0);
}

void
RemoteNode::reset_()
{
    LOCK();

    zock_.reset(new zmq::socket_t(ztx_, ZMQ_DEALER));
    ZUtils::socket_no_linger(*zock_);

    LOG_INFO(node_id() << ": connecting to " << uri());
    zock_->connect(boost::lexical_cast<std::string>(uri()).c_str());

    submitted_work_.clear();
    drop_keepalive_work_();
    arm_keepalive_timer_(vrouter_.keepalive_time());
    missing_keepalive_probes_ = 0;
}

namespace
{

vfsprotocol::Tag
allocate_tag()
{
    static std::atomic<uint64_t> t(yt::SourceOfUncertainty()(static_cast<uint64_t>(0)));
    return vfsprotocol::Tag(t++);
}

}

struct RemoteNode::WorkItem
{
    const google::protobuf::Message& request;
    const vfsprotocol::Tag request_tag;
    vfsprotocol::RequestType request_type;
    const char* request_desc;

    ExtraSendFun extra_send_fun;
    ExtraRecvFun extra_recv_fun;

    boost::promise<vfsprotocol::ResponseType> promise;
    boost::unique_future<vfsprotocol::ResponseType> future;

    template<typename Request>
    WorkItem(const Request& req,
             ExtraSendFun extra_send,
             ExtraRecvFun extra_recv)
        : request(req)
        , request_tag(allocate_tag())
        , request_type(vfsprotocol::RequestTraits<Request>::request_type)
        , request_desc(vfsprotocol::request_type_to_string(request_type))
        , extra_send_fun(std::move(extra_send))
        , extra_recv_fun(std::move(extra_recv))
          // clang++ (3.8.0-2ubuntu3~trusty4) complains otherwise about
          // 'promise' being used unitialized when initializing 'future'
        , promise()
        , future(promise.get_future())
    {}
};

void
RemoteNode::drop_request_(const WorkItem& work)
{
    LOG_TRACE(node_id() << ": dropping request " <<
              work.request_desc << ", tag " <<
              work.request_tag);

    LOCK();
    queued_work_.remove_if([&](const WorkItemPtr& w)
                           {
                               VERIFY(w);
                               return w->request_tag == work.request_tag;
                           });
    submitted_work_.erase(work.request_tag);
}

template<typename Request>
void
RemoteNode::handle_(const Request& req,
                    const bc::milliseconds& timeout_ms,
                    ExtraSendFun extra_send,
                    ExtraRecvFun extra_recv)
{
    auto work = boost::make_shared<WorkItem>(req,
                                             std::move(extra_send),
                                             std::move(extra_recv));

    {
        LOCK();
        queued_work_.push_back(work);
        notify_();
    }

    try
    {
        switch (work->future.wait_for(timeout_ms))
        {
        case boost::future_status::deferred:
            {
                LOG_ERROR(node_id() << ": " << work->request_desc << ", tag " <<
                          work->request_tag << ": future unexpectedly returned 'deferred' status");
                VERIFY(0 == "unexpected future_status 'deferred'");
            }
        case boost::future_status::timeout:
            {
                LOG_INFO(node_id() << ": remote did not respond within " << timeout_ms <<
                         " milliseconds - giving up");
                throw RequestTimeoutException("request to remote node timed out");
            }
        case boost::future_status::ready:
            {
                break;
            }
        }
    }
    catch (...)
    {
        drop_request_(*work);
        throw;
    }

    handle_response_(*work);
}

void
RemoteNode::handle_response_(WorkItem& work)
{
    const vfsprotocol::ResponseType rsp_type = work.future.get();
    switch (rsp_type)
    {
    case vfsprotocol::ResponseType::Ok:
        // all is well
        break;
    case vfsprotocol::ResponseType::ObjectNotRunningHere:
        LOG_INFO(node_id() << ": volume not present on that node");
        throw vd::VolManager::VolumeDoesNotExistException("volume not present on node",
                                                          work.request_desc);
        break;
    case vfsprotocol::ResponseType::UnknownRequest:
        LOG_WARN(node_id() << ": got an UnknownRequest response status in response to " <<
                 work.request_desc);
        // handle differently once we need to take care of backward compatibility.
        throw ProtocolError("Remote sent UnknownRequest response status",
                            work.request_desc);
        break;
    case vfsprotocol::ResponseType::Timeout:
        LOG_ERROR(node_id() << ": got a Timeout response status in response to " <<
                  work.request_desc);
        throw RemoteTimeoutException("Remote sent timeout status",
                                     work.request_desc);
        break;
    case vfsprotocol::ResponseType::AccessBeyondEndOfVolume:
        LOG_ERROR(node_id() << ": got an access beyond volume boundaries in response to " <<
                  work.request_desc);
        throw vd::AccessBeyondEndOfVolumeException("Access beyond volume boundaries");
        break;
    case vfsprotocol::ResponseType::CannotShrinkVolume:
        LOG_ERROR(node_id() << ": cannot shrink volume in response to " <<
                  work.request_desc);
        throw vd::CannotShrinkVolumeException("Cannot shrink volume");
        break;
    case vfsprotocol::ResponseType::CannotGrowVolumeBeyondLimit:
        LOG_ERROR(node_id() << ": cannot grow volume beyond limit in response to " <<
                  work.request_desc);
        throw vd::CannotGrowVolumeBeyondLimitException("Cannot grow volume beyond limit");
        break;
    default:
        LOG_ERROR(node_id() << ": " << work.request_desc <<
                  " failed, remote returned status " << vfsprotocol::response_type_to_string(rsp_type) <<
                  " (" << static_cast<uint32_t>(rsp_type) << ")");
        throw fungi::IOException("Remote operation failed",
                                 work.request_desc);
        break;
    }
}

void
RemoteNode::arm_keepalive_timer_(const bc::seconds& timeout)
{
    itimerspec its;
    memset(&its, 0x0, sizeof(its));
    its.it_value.tv_sec = timeout.count();

    VERIFY(timer_fd_ >= 0);

    int ret = timerfd_settime(timer_fd_,
                              0,
                              &its,
                              nullptr);
    if (ret < 0)
    {
        LOG_ERROR("Failed to arm keepalive timer with timeout " << timeout <<
                  ": " << strerror(errno));
    }
}

bc::seconds
RemoteNode::check_keepalive_()
{
    bc::seconds timeout(vrouter_.keepalive_interval());

    if (keepalive_work_ and not keepalive_work_->future.is_ready())
    {
        ++missing_keepalive_probes_;

        LOG_WARN(node_id() << ": keepalive probe " <<
                 missing_keepalive_probes_ << " pending");

        if (missing_keepalive_probes_ > vrouter_.keepalive_retries())
        {
            LOG_ERROR(node_id() << ": missing keepalive probes (" <<
                      missing_keepalive_probes_ << ") > max (" <<
                      vrouter_.keepalive_retries() << ")");

            keepalive_work_ = nullptr;

            // *Ugly* hack - boost has no std::make_exception_ptr equivalent (OTOH std::future does
            // not have ->is_ready() which renders it completely useless for this use case), so
            // we have to throw/catch/throw.
            try
            {
                boost::throw_exception(ClusterNodeNotReachableException("cluster node not reachable",
                                                                        node_id().str().c_str()));
            }
            catch (...)
            {
                LOCK();
                for (auto& p : submitted_work_)
                {
                    LOG_TRACE(node_id() << ": request " << p.second->request_desc <<
                              ", tag " << p.second->request_tag);
                    p.second->promise.set_exception(boost::current_exception());
                }

                // submitted_work_ is cleared / the socket is reset and the timer rearmed
                // by the caller (event loop)
                throw;
            }
        }
        else
        {
            LOCK();
            drop_keepalive_work_();
        }
    }
    else
    {
        timeout = vrouter_.keepalive_time();
        missing_keepalive_probes_ = 0;
    }

    return timeout;
}

void
RemoteNode::keepalive_()
{
    const bc::seconds timeout(check_keepalive_());

    ExtraRecvFun extra_recv([&]
                            {
                                ZEXPECT_MORE(*zock_, "PingMessage");
                                vfsprotocol::PingMessage rsp;
                                ZUtils::deserialize_from_socket(*zock_, rsp);

                                rsp.CheckInitialized();
                                LOG_INFO("got keepalive rsp from " << rsp.sender_id());
                            });

    keepalive_work_ =
        boost::make_shared<WorkItem>(keepalive_probe_,
                                     ExtraSendFun(),
                                     std::move(extra_recv));


    LOCK();
    submit_work_(keepalive_work_);
    arm_keepalive_timer_(timeout);
}

void
RemoteNode::drop_keepalive_work_()
{
    ASSERT_LOCKABLE_LOCKED(work_lock_);

    if (keepalive_work_)
    {
        submitted_work_.erase(keepalive_work_->request_tag);
        keepalive_work_ = nullptr;
    }
}

void
RemoteNode::work_()
{
    {
        LOCK();
        // the socket was created by another thread, per ZMQs documentation ownership
        // transfer requires a full fence memory barrier which this lock should offer
        LOG_INFO(node_id() << ": starting worker, keepalive settings:" <<
                 vrouter_.keepalive_time() << "/" <<
                 vrouter_.keepalive_interval() << "/" <<
                 vrouter_.keepalive_retries());
    }

    std::array<zmq::pollitem_t, 3> items;
    bool wait_for_write = false;

    arm_keepalive_timer_(vrouter_.keepalive_time());

    auto keepalive_is_enabled([&]() -> bool
                              {
                                  return vrouter_.keepalive_time() != bc::seconds(0);
                              });

    while (true)
    {
        try
        {
            ASSERT(zock_ != nullptr);
            const bool keepalive_was_disabled = not keepalive_is_enabled();

            items[0].socket = *zock_;
            items[0].fd = -1;
            items[0].events = ZMQ_POLLIN;
            if (wait_for_write)
            {
                items[0].events |= ZMQ_POLLOUT;
            }

            items[1].socket = nullptr;
            items[1].fd = event_fd_;
            items[1].events = ZMQ_POLLIN;

            items[2].socket = nullptr;
            items[2].fd = timer_fd_;
            items[2].events = ZMQ_POLLIN;

            const int ret = zmq::poll(items.data(),
                                      items.size(),
                                      30000);
            THROW_WHEN(ret < 0);

            if (stop_)
            {
                LOCK();
                drop_keepalive_work_();
                ASSERT(queued_work_.empty());
                ASSERT(submitted_work_.empty());
                break;
            }
            else
            {
                if ((items[2].revents bitand ZMQ_POLLIN))
                {
                    keepalive_();
                }
                if ((items[0].revents bitand ZMQ_POLLIN))
                {
                    recv_responses_();
                }

                if ((items[0].revents bitand ZMQ_POLLOUT))
                {
                    wait_for_write = not send_requests_();
                }

                if ((items[1].revents bitand ZMQ_POLLIN))
                {
                    wait_for_write = not send_requests_();
                }
            }

            if (keepalive_was_disabled and keepalive_is_enabled())
            {
                arm_keepalive_timer_(vrouter_.keepalive_time());
            }
        }
        catch (zmq::error_t& e)
        {
            if (e.num() == ETERM)
            {
                LOG_INFO(node_id() << ": stop requested, breaking out of the loop");
                return;
            }
            else
            {
                LOG_ERROR(node_id() << ": caught ZMQ exception in event loop: " <<
                          e.what() << ". Resetting");
                reset_();
            }
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR(node_id() << ": caught exception in event loop: " <<
                          EWHAT << ". Resetting.");
                reset_();
            });
    }
}

bool
RemoteNode::send_requests_()
{
    int ret;
    eventfd_t val = 0;

    do {
        ret = eventfd_read(event_fd_,
                           &val);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0)
    {
        VERIFY(errno == EAGAIN);
    }
    else
    {
        VERIFY(ret == 0);
        VERIFY(val > 0);
    }

    while (true)
    {
        LOCK();

        if (queued_work_.empty())
        {
            return true;
        }

        if (not ZUtils::writable(*zock_))
        {
            LOG_WARN(node_id() << ": queuing limit reached");
            return false;
        }

        WorkItemPtr work = queued_work_.front();
        VERIFY(work);

        queued_work_.pop_front();

        submit_work_(work);
    }
}

void
RemoteNode::submit_work_(const WorkItemPtr& work)
{
    ASSERT_LOCKABLE_LOCKED(work_lock_);

    bool ok = false;
    std::tie(std::ignore, ok) = submitted_work_.emplace(work->request_tag,
                                                        work);
    VERIFY(ok);

    ZUtils::send_delimiter(*zock_, MoreMessageParts::T);
    ZUtils::serialize_to_socket(*zock_, work->request_type, MoreMessageParts::T);
    ZUtils::serialize_to_socket(*zock_, work->request_tag, MoreMessageParts::T);
    ZUtils::serialize_to_socket(*zock_,
                                work->request,
                                (work->extra_send_fun) ?
                                MoreMessageParts::T :
                                MoreMessageParts::F);

    if (work->extra_send_fun)
    {
        work->extra_send_fun();
    }

    LOG_TRACE(node_id() << ": sent " << work->request_desc << ", tag " << work->request_tag <<
              ", extra: " << (work->extra_send_fun != nullptr));
}

void
RemoteNode::recv_responses_()
{
    while (ZUtils::readable(*zock_))
    {
        ZUtils::recv_delimiter(*zock_);
        ZEXPECT_MORE(*zock_, "response type");

        vfsprotocol::ResponseType rsp_type;
        ZUtils::deserialize_from_socket(*zock_, rsp_type);
        ZEXPECT_MORE(*zock_, "response tag");

        vfsprotocol::Tag rsp_tag;
        ZUtils::deserialize_from_socket(*zock_, rsp_tag);

        LOG_TRACE(node_id() << ": recv'd " << static_cast<uint32_t>(rsp_type) << ", tag " << rsp_tag);

        LOCK();

        auto it = submitted_work_.find(rsp_tag);
        if (it == submitted_work_.end())
        {
            LOG_WARN(node_id() << ": received orphan response with tag " <<
                     rsp_tag << " - did we time out?");
            ZUtils::drop_remaining_message_parts(*zock_);
        }
        else
        {
            WorkItemPtr w = it->second;
            submitted_work_.erase(it);

            try
            {
                if (rsp_type == vfsprotocol::ResponseType::Ok and
                    w->extra_recv_fun)
                {
                    w->extra_recv_fun();
                }

                ZEXPECT_NOTHING_MORE(*zock_);
                w->promise.set_value(rsp_type);
            }
            catch (...)
            {
                ZUtils::drop_remaining_message_parts(*zock_);
                w->promise.set_exception(boost::current_exception());
            }
        }
    }
}

void
RemoteNode::read(const Object& obj,
                 uint8_t* buf,
                 size_t* size,
                 off_t off)
{
    ASSERT(size);

    LOG_TRACE(node_id() << ": obj " << obj.id << ", size " << *size << ", off " << off);

    const auto req(vfsprotocol::MessageUtils::create_read_request(obj, *size, off));

    ExtraRecvFun recv_data([&]
                           {
                               ZEXPECT_MORE(*zock_, "read data");

                               zmq::message_t msg;
                               zock_->recv(&msg);

                               if (msg.size() > *size)
                               {
                                   LOG_ERROR(node_id() << ": read " << msg.size() <<
                                             " > expected " << *size << " from " << obj.id);
                                   throw fungi::IOException("Read size mismatch",
                                                            obj.id.str().c_str());
                               }

                               *size = msg.size();
                               memcpy(buf, msg.data(), *size);
                           });

    handle_(req,
            vrouter_.redirect_timeout(),
            ExtraSendFun(),
            std::move(recv_data));
}

void
RemoteNode::write(const Object& obj,
                  const uint8_t* buf,
                  size_t* size,
                  off_t off,
                  vd::DtlInSync& dtl_in_sync)
{
    ASSERT(size);

    LOG_TRACE(node_id() << ": obj " << obj.id << ", size " << *size << ", off " << off);

    const auto req(vfsprotocol::MessageUtils::create_write_request(obj, *size, off));

    ExtraSendFun send_data([&]
                           {
                               zmq::message_t msg(*size);
                               memcpy(msg.data(), buf, *size);
                               zock_->send(msg, 0);
                           });

    ExtraRecvFun get_rsp([&]
                         {
                             ZEXPECT_MORE(*zock_, "WriteResponse");

                             vfsprotocol::WriteResponse rsp;
                             ZUtils::deserialize_from_socket(*zock_, rsp);

                             rsp.CheckInitialized();
                             *size = rsp.size();
                             dtl_in_sync = rsp.dtl_in_sync() ? vd::DtlInSync::T : vd::DtlInSync::F;
                         });

    handle_(req,
            vrouter_.redirect_timeout(),
            std::move(send_data),
            std::move(get_rsp));
}

void
RemoteNode::sync(const Object& obj,
                 vd::DtlInSync& dtl_in_sync)
{
    LOG_TRACE(node_id() << ": obj " << obj.id);

    const auto req(vfsprotocol::MessageUtils::create_sync_request(obj));

    ExtraRecvFun get_rsp([&]
                         {
                             // the SyncResponse data was introduced at a later point
                             // so older versions might not send it.
                             if (ZUtils::more_message_parts(*zock_))
                             {
                                 vfsprotocol::SyncResponse rsp;
                                 ZUtils::deserialize_from_socket(*zock_, rsp);
                                 rsp.CheckInitialized();
                                 dtl_in_sync = rsp.dtl_in_sync() ? vd::DtlInSync::T : vd::DtlInSync::F;
                             }
                             else
                             {
                                 dtl_in_sync = vd::DtlInSync::F;
                             }
                         });

    handle_(req,
            vrouter_.redirect_timeout(),
            ExtraSendFun(),
            std::move(get_rsp));
}

uint64_t
RemoteNode::get_size(const Object& obj)
{
    LOG_TRACE(node_id() << ": obj " << obj.id);

    uint64_t size = 0;
    ExtraRecvFun get_size([&]
                          {
                              ZEXPECT_MORE(*zock_, "GetSizeResponse");

                              vfsprotocol::GetSizeResponse rsp;
                              ZUtils::deserialize_from_socket(*zock_, rsp);

                              rsp.CheckInitialized();
                              size = rsp.size();
                          });

    const auto req(vfsprotocol::MessageUtils::create_get_size_request(obj));

    handle_(req,
            vrouter_.redirect_timeout(),
            ExtraSendFun(),
            std::move(get_size));

    return size;
}

vd::ClusterMultiplier
RemoteNode::get_cluster_multiplier(const Object& obj)
{
    LOG_TRACE(node_id() << ": obj " << obj.id);

    uint32_t size = 0;
    ExtraRecvFun get_cluster_multiplier([&]
                          {
                              ZEXPECT_MORE(*zock_,
                                           "GetClusterMultiplierResponse");

                              vfsprotocol::GetClusterMultiplierResponse rsp;
                              ZUtils::deserialize_from_socket(*zock_, rsp);

                              rsp.CheckInitialized();
                              size = rsp.size();
                          });

    const auto req(vfsprotocol::MessageUtils::create_get_cluster_multiplier_request(obj));

    handle_(req,
            vrouter_.redirect_timeout(),
            ExtraSendFun(),
            std::move(get_cluster_multiplier));

    return vd::ClusterMultiplier(size);
}

vd::CloneNamespaceMap
RemoteNode::get_clone_namespace_map(const Object& obj)
{
    LOG_TRACE(node_id() << ": obj " << obj.id);

    vd::CloneNamespaceMap cnmap;
    ExtraRecvFun get_clone_namespace_map([&]
                          {
                            ZEXPECT_MORE(*zock_,
                                         "GetCloneNamespaceMapResponse");

                            vfsprotocol::GetCloneNamespaceMapResponse rsp;
                            ZUtils::deserialize_from_socket(*zock_, rsp);

                            rsp.CheckInitialized();
                            for (int i = 0; i < rsp.map_entry_size(); i++)
                            {
                                const auto& e = rsp.map_entry(i);
                                cnmap.emplace(vd::SCOCloneID(e.clone_id()),
                                               backend::Namespace(e.ns()));
                            }
                          });

    const auto req(vfsprotocol::MessageUtils::create_get_clone_namespace_map_request(obj));

    handle_(req,
            vrouter_.redirect_timeout(),
            ExtraSendFun(),
            std::move(get_clone_namespace_map));

    return cnmap;
}

std::vector<vd::ClusterLocation>
RemoteNode::get_page(const Object& obj,
                     const vd::ClusterAddress ca)
{
    LOG_TRACE(node_id() << ": obj " << obj.id << ", ca " << ca);

    std::vector<vd::ClusterLocation> cloc;
    ExtraRecvFun get_page([&]
    {
      ZEXPECT_MORE(*zock_,
                   "GetPageResponse");

      vfsprotocol::GetPageResponse rsp;
      ZUtils::deserialize_from_socket(*zock_, rsp);

      rsp.CheckInitialized();
      for (int i = 0; i < rsp.cluster_location_size(); i++)
      {
           vd::ClusterLocation cl;
           *reinterpret_cast<uint64_t*>(&cl) = rsp.cluster_location(i);
           cloc.emplace_back(cl);
      }
    });

    const auto req(vfsprotocol::MessageUtils::create_get_page_request(obj,
                                                                      ca));

    handle_(req,
            vrouter_.redirect_timeout(),
            ExtraSendFun(),
            std::move(get_page));

    return cloc;
}

void
RemoteNode::resize(const Object& obj,
                   uint64_t newsize)
{
    LOG_TRACE(node_id() << ": obj " << obj.id << ", new size " << newsize);

    const auto req(vfsprotocol::MessageUtils::create_resize_request(obj, newsize));

    handle_(req,
            vrouter_.redirect_timeout());
}

void
RemoteNode::unlink(const Object& obj)
{
    LOG_TRACE(node_id() << ": obj " << obj.id);

    const auto req(vfsprotocol::MessageUtils::create_delete_request(obj));

    handle_(req,
            vrouter_.redirect_timeout());
}

void
RemoteNode::transfer(const Object& obj)
{
    LOG_TRACE(node_id() << ": obj " << obj.id);

    const auto req(vfsprotocol::MessageUtils::create_transfer_request(obj,
                                                                      vrouter_.node_id(),
                                                                      vrouter_.backend_sync_timeout()));

    const bc::milliseconds req_timeout(vrouter_.backend_sync_timeout() + vrouter_.migrate_timeout());
    handle_(req,
            req_timeout);
}

void
RemoteNode::ping()
{
    LOG_TRACE(node_id());

    const auto req(vfsprotocol::MessageUtils::create_ping_message(vrouter_.node_id()));

    ExtraRecvFun handle_pong([&]
                             {
                                 ZEXPECT_MORE(*zock_, "PingMessage");
                                 vfsprotocol::PingMessage rsp;
                                 ZUtils::deserialize_from_socket(*zock_, rsp);

                                 rsp.CheckInitialized();
                                 LOG_TRACE("got pong from " << rsp.sender_id());
                             });

    handle_(req,
            vrouter_.redirect_timeout(),
            ExtraSendFun(),
            std::move(handle_pong));
}

}
