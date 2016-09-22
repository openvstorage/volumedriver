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
#include "Protocol.h"
#include "RemoteNode.h"
#include "ObjectRouter.h"
#include "ZUtils.h"

#include <boost/lexical_cast.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>

// XXX: needed as we throw VolManager::VolumeDoesNotExistException.
// This needs to be redone, we don't want to know about VolManager
// at this level
#include <volumedriver/VolManager.h>

namespace volumedriverfs
{

namespace vd = volumedriver;

#define LOCK()                                  \
    std::lock_guard<lock_type> lg__(lock_)

RemoteNode::RemoteNode(ObjectRouter& vrouter,
                       const ClusterNodeConfig& cfg,
                       std::shared_ptr<zmq::context_t> ztx)
    : ClusterNode(vrouter, cfg)
    , ztx_(ztx)
{
    LOCK();
    init_zock_();
}

void
RemoteNode::init_zock_()
{
    VERIFY(ztx_ != nullptr);
    ASSERT_LOCKABLE_LOCKED(lock_);

    const std::string rport(boost::lexical_cast<std::string>(config.message_port));
    zock_.reset(new zmq::socket_t(*ztx_, ZMQ_REQ));

    const std::string tcp_addr("tcp://" + config.message_host + std::string(":") + rport);
    ZUtils::socket_no_linger(*zock_);

    LOG_INFO("Connecting to " << tcp_addr);
    zock_->connect(tcp_addr.c_str());
}

void
RemoteNode::wait_for_remote_(const boost::chrono::milliseconds& timeout_ms)
{
    ASSERT_LOCKABLE_LOCKED(lock_);

    LOG_TRACE("waiting for response from remote " << config.vrouter_id <<
              ", timeout: " << timeout_ms << " ms");

    ASSERT(zock_ != nullptr);

    zmq::pollitem_t item;
    item.socket = *zock_;
    item.events = ZMQ_POLLIN;

    // TODO: ZMQ exceptions are passed on - are there cases where we don't
    // want that, e.g. EINTR?
    const int ret = zmq::poll(&item,
                              1,
                              timeout_ms.count() == 0 ?
                              - 1 :
                              timeout_ms.count());
    THROW_WHEN(ret < 0);

    if (ret == 0)
    {
        LOG_INFO("Remote did not respond within " << timeout_ms <<
                 " milliseconds - giving up");

        init_zock_();
        throw RequestTimeoutException("request to remote node timed out");
    }
}

template<typename Request>
void
RemoteNode::handle_(const Request& req,
                    const boost::chrono::milliseconds& timeout_ms,
                    ExtraSendFun* send_extra,
                    ExtraRecvFun* recv_extra)
{
    const vfsprotocol::RequestType req_type =
        vfsprotocol::RequestTraits<Request>::request_type;

    const char* req_desc = vfsprotocol::request_type_to_string(req_type);
    LOG_TRACE(req_desc);

    LOCK();

    ASSERT(zock_ != nullptr);

    ZUtils::serialize_to_socket(*zock_, req_type, MoreMessageParts::T);

    vfsprotocol::Tag req_tag(reinterpret_cast<uint64_t>(&req));
    ZUtils::serialize_to_socket(*zock_, req_tag, MoreMessageParts::T);

    ZUtils::serialize_to_socket(*zock_,
                                req,
                                (send_extra != nullptr) ?
                                MoreMessageParts::T :
                                MoreMessageParts::F);

    if (send_extra != nullptr)
    {
        (*send_extra)();
    }

    LOG_TRACE("sent " << req_desc << ", tag " << req_tag <<
              ", extra: " << (send_extra != nullptr) << ", timeout ms: " << timeout_ms);

    wait_for_remote_(timeout_ms);

    vfsprotocol::ResponseType rsp_type;
    ZUtils::deserialize_from_socket(*zock_, rsp_type);

    ZEXPECT_MORE(*zock_, "response tag");

    vfsprotocol::Tag rsp_tag;
    ZUtils::deserialize_from_socket(*zock_, rsp_tag);

    const char* rsp_desc = vfsprotocol::response_type_to_string(rsp_type);
    LOG_TRACE("recv'd " << rsp_desc << ", tag " << rsp_tag);

    if (rsp_tag != req_tag)
    {
        LOG_ERROR("req " << req_desc << ", rsp " << rsp_desc <<
                  ": expected tag " << req_tag << ", got " << rsp_tag);
        throw ProtocolError("Wrong tag in response");
    }

    try
    {
        switch (rsp_type)
        {
        case vfsprotocol::ResponseType::Ok:
            if (recv_extra != nullptr)
            {
                (*recv_extra)();
            }
            ZEXPECT_NOTHING_MORE(*zock_);
            break;
        case vfsprotocol::ResponseType::ObjectNotRunningHere:
            ZEXPECT_NOTHING_MORE(*zock_);
            LOG_INFO("volume not present on node " << config.vrouter_id);

            throw vd::VolManager::VolumeDoesNotExistException("volume not present on node",
                                                              req_desc);
            break;
        case vfsprotocol::ResponseType::UnknownRequest:
            ZEXPECT_NOTHING_MORE(*zock_);
            LOG_WARN("got an UnknownRequest response status in response to " <<
                     req_desc);
            // handle differently once we need to take care of backward compatibility.
            throw ProtocolError("Remote sent UnknownRequest response status",
                                req_desc);
            break;
        case vfsprotocol::ResponseType::Timeout:
            ZEXPECT_NOTHING_MORE(*zock_);
            LOG_ERROR("got a Timeout response status in response to " <<
                      req_desc);
            throw RemoteTimeoutException("Remote sent timeout status",
                                         req_desc);
            break;
        default:
            ZEXPECT_NOTHING_MORE(*zock_);
            LOG_ERROR(req_desc << " failed, remote returned status " <<
                      rsp_desc << " (" << static_cast<uint32_t>(rsp_type) << ")");
            throw fungi::IOException("Remote operation failed",
                                     req_desc);
        }
    }
    catch (...)
    {
        ZUtils::drop_remaining_message_parts(*zock_);
        throw;
    }
}

void
RemoteNode::read(const Object& obj,
                 uint8_t* buf,
                 size_t* size,
                 off_t off)
{
    ASSERT(size);

    LOG_TRACE(obj.id << ": remote node " << config.vrouter_id <<
              ", size " << *size << ", off " << off);

    const auto req(vfsprotocol::MessageUtils::create_read_request(obj, *size, off));

    ExtraRecvFun recv_data([&]
                           {
                               ZEXPECT_MORE(*zock_, "read data");

                               zmq::message_t msg;
                               zock_->recv(&msg);

                               if (msg.size() > *size)
                               {
                                   LOG_ERROR("Read " << msg.size() << " > expected " <<
                                             *size << " from " << obj.id);
                                   throw fungi::IOException("Read size mismatch",
                                                            obj.id.str().c_str());
                               }

                               *size = msg.size();
                               memcpy(buf, msg.data(), *size);
                           });

    handle_(req,
            vrouter_.redirect_timeout(),
            nullptr,
            &recv_data);
}

void
RemoteNode::write(const Object& obj,
                  const uint8_t* buf,
                  size_t* size,
                  off_t off)
{
    ASSERT(size);

    LOG_TRACE(obj.id << ": remote node " << config.vrouter_id <<
              ", size " << *size << ", off " << off);

    const auto req(vfsprotocol::MessageUtils::create_write_request(obj, *size, off));

    ExtraSendFun send_data([&]
                           {
                               zmq::message_t msg(*size);
                               memcpy(msg.data(), buf, *size);
                               zock_->send(msg, 0);
                           });

    ExtraRecvFun get_size([&]
                          {
                              ZEXPECT_MORE(*zock_, "WriteResponse");

                              vfsprotocol::WriteResponse rsp;
                              ZUtils::deserialize_from_socket(*zock_, rsp);

                              rsp.CheckInitialized();
                              *size = rsp.size();
                          });

    handle_(req,
            vrouter_.redirect_timeout(),
            &send_data,
            &get_size);
}

void
RemoteNode::sync(const Object& obj)
{
    LOG_TRACE(obj.id);

    const auto req(vfsprotocol::MessageUtils::create_sync_request(obj));

    handle_(req,
            vrouter_.redirect_timeout());
}

uint64_t
RemoteNode::get_size(const Object& obj)
{
    LOG_TRACE(obj.id);

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
            nullptr,
            &get_size);

    return size;
}

void
RemoteNode::resize(const Object& obj,
                   uint64_t newsize)
{
    LOG_TRACE(obj.id << ": new size " << newsize);

    const auto req(vfsprotocol::MessageUtils::create_resize_request(obj, newsize));

    handle_(req,
            vrouter_.redirect_timeout());
}

void
RemoteNode::unlink(const Object& obj)
{
    LOG_TRACE(obj.id << ": unlinking");

    const auto req(vfsprotocol::MessageUtils::create_delete_request(obj));

    handle_(req,
            vrouter_.redirect_timeout());
}

void
RemoteNode::transfer(const Object& obj)
{
    LOG_TRACE(obj.id << ", transfer volume");

    const auto req(vfsprotocol::MessageUtils::create_transfer_request(obj,
                                                                      vrouter_.node_id(),
                                                                      vrouter_.backend_sync_timeout()));

    const boost::chrono::milliseconds req_timeout =
        vrouter_.backend_sync_timeout().count() ?
        vrouter_.backend_sync_timeout() + vrouter_.migrate_timeout() :
        vrouter_.backend_sync_timeout();

    handle_(req,
            req_timeout);
}

void
RemoteNode::ping()
{
    LOG_TRACE("ping requested");
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
            nullptr,
            &handle_pong);
}

}
