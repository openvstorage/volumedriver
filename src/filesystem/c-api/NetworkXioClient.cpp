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

#include "Logger.h"
#include "MsgpackAdaptors.h"
#include "NetworkXioClient.h"
#include "common_priv.h"

#include <youtils/Assert.h>

#include <thread>
#include <future>
#include <functional>
#include <atomic>
#include <system_error>

PRAGMA_IGNORE_WARNING_BEGIN("-Wctor-dtor-privacy")
#include <msgpack.hpp>
PRAGMA_IGNORE_WARNING_END;

namespace yt = youtils;

std::atomic<int> xio_init_refcnt =  ATOMIC_VAR_INIT(0);

static inline void
xrefcnt_init()
{
    if (xio_init_refcnt.fetch_add(1, std::memory_order_relaxed) == 0)
    {
        xio_init();
    }
}

static inline void
xrefcnt_shutdown()
{
    if (xio_init_refcnt.fetch_sub(1, std::memory_order_relaxed) == 1)
    {
        xio_shutdown();
    }
}

namespace libovsvolumedriver
{

template<class T>
static int
static_on_session_event(xio_session *session,
                        xio_session_event_data *event_data,
                        void *cb_user_context)
{
    T *obj = reinterpret_cast<T*>(cb_user_context);
    if (obj == NULL)
    {
        assert(obj != NULL);
        return -1;
    }
    return obj->on_session_event(session, event_data);
}

template<class T>
static int
static_on_response(xio_session *session,
                   xio_msg *req,
                   int last_in_rxq,
                   void *cb_user_context)
{
    T *obj = reinterpret_cast<T*>(cb_user_context);
    if (obj == NULL)
    {
        assert(obj != NULL);
        return -1;
    }
    return obj->on_response(session, req, last_in_rxq);
}

namespace
{
std::string xio_ka_time("LIBOVSVOLUMEDRIVER_XIO_KEEPALIVE_TIME");
std::string xio_ka_intvl("LIBOVSVOLUMEDRIVER_XIO_KEEPALIVE_INTVL");
std::string xio_ka_probes("LIBOVSVOLUMEDRIVER_XIO_KEEPALIVE_PROBES");
std::string xio_poll_timeout_us("LIBOVSVOLUMEDRIVER_XIO_POLLING_TIMEOUT_US");
std::string xio_ctrl_timeout_ms("LIBOVSVOLUMEDRIVER_XIO_CONTROL_TIMEOUT_MS");
}

template<class T>
static int
static_on_msg_error(xio_session *session,
                    xio_status error,
                    xio_msg_direction direction,
                    xio_msg *msg,
                    void *cb_user_context)
{
    T *obj = reinterpret_cast<T*>(cb_user_context);
    if (obj == NULL)
    {
        assert(obj != NULL);
        return -1;
    }
    return obj->on_msg_error(session, error, direction, msg);
}

template<class T>
static void
static_evfd_stop_loop(int fd, int events, void *data)
{
    T *obj = reinterpret_cast<T*>(data);
    if (obj == NULL)
    {
        assert(obj != NULL);
        return;
    }
    obj->evfd_stop_loop(fd, events, data);
}

NetworkXioClient::NetworkXioClient(const std::string& uri,
                                   const uint64_t qd,
                                   NetworkHAContext& ha_ctx)
    : uri_(uri)
    , stopping(false)
    , stopped(false)
    , disconnected(false)
    , disconnecting(false)
    , connect_error(false)
    , nr_req_queue(qd)
    , evfd()
    , ha_ctx_(ha_ctx)
    , dtl_in_sync_(true)
{
    LIBLOGID_INFO("uri: " << uri << ", queue depth: " << qd);
    ses_ops.on_session_event = static_on_session_event<NetworkXioClient>;
    ses_ops.on_session_established = NULL;
    ses_ops.on_msg = static_on_response<NetworkXioClient>;
    ses_ops.on_msg_error = static_on_msg_error<NetworkXioClient>;
    ses_ops.on_cancel_request = NULL;
    ses_ops.assign_data_in_buf = NULL;

    memset(&params, 0, sizeof(params));
    memset(&cparams, 0, sizeof(cparams));

    params.type = XIO_SESSION_CLIENT;
    params.ses_ops = &ses_ops;
    params.user_context = this;
    params.uri = uri_.c_str();

    xrefcnt_init();

    std::promise<bool> promise;
    std::future<bool> future(promise.get_future());

    try
    {
        xio_thread_ = std::thread([&](){
                try
                {
                    run(promise);
                }
                catch (...)
                {
                    try
                    {
                        promise.set_exception(std::current_exception());
                    } catch(...){}
                }
        });
    }
    catch (const std::system_error& e)
    {
        LIBLOGID_ERROR("failed to create xio thread: " << e.what());
        throw XioClientCreateException("failed to create XIO worker thread");
    }

    try
    {
        future.get();
    }
    catch (const XioClientCreateException& e)
    {
        xio_thread_.join();
        throw;
    }
    catch (const XioClientRegHandlerException& e)
    {
        xio_thread_.join();
        throw;
    }
    catch (const std::exception& e)
    {
        LIBLOGID_ERROR("failed to create xio thread: " << e.what());
        xio_thread_.join();
        throw XioClientCreateException("failed to create XIO worker thread");
    }
    catch (...)
    {
        LIBLOGID_ERROR("failed to create xio thread, unknown exception ");
        xio_thread_.join();
        throw XioClientCreateException("failed to create XIO worker thread");
    }
}

void
NetworkXioClient::run(std::promise<bool>& promise)
{
    int xopt = 0, optlen;
    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_ENABLE_FLOW_CONTROL,
                &xopt, sizeof(int));

    xopt = 2 * nr_req_queue;
    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_SND_QUEUE_DEPTH_MSGS,
                &xopt,
                sizeof(int));

    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_RCV_QUEUE_DEPTH_MSGS,
                &xopt,
                sizeof(int));

    struct xio_options_keepalive ka;
    optlen = sizeof(ka);
    xio_get_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_CONFIG_KEEPALIVE,
                &ka,
                &optlen);

    ka.time = yt::System::get_env_with_default<int>(xio_ka_time,
                                                    ka.time);
    ka.intvl = yt::System::get_env_with_default<int>(xio_ka_intvl,
                                                     ka.intvl);
    ka.probes = yt::System::get_env_with_default<int>(xio_ka_probes,
                                                      ka.probes);

    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_CONFIG_KEEPALIVE,
                &ka,
                sizeof(ka));

    xopt = 1;
    xio_set_opt(NULL,
                XIO_OPTLEVEL_TCP,
                XIO_OPTNAME_TCP_NO_DELAY,
                &xopt,
                sizeof(xopt));

    try
    {
        int polling_timeout_us =
            yt::System::get_env_with_default<int>(xio_poll_timeout_us, 0);
        ctx = std::shared_ptr<xio_context>(
                            xio_context_create(NULL, polling_timeout_us, -1),
                            xio_destroy_ctx_shutdown);
    }
    catch (const std::bad_alloc& e)
    {
        LIBLOGID_ERROR(e.what());
        xrefcnt_shutdown();
        throw;
    }

    if (ctx == nullptr)
    {
        LIBLOGID_ERROR("failed to create XIO context: " <<
                       xio_strerror(xio_errno()));
        throw XioClientCreateException("failed to create XIO context",
                                       nullptr,
                                       xio_errno());
    }

    if(xio_context_add_ev_handler(ctx.get(),
                                  evfd,
                                  XIO_POLLIN,
                                  static_evfd_stop_loop<NetworkXioClient>,
                                  this))
    {
        LIBLOGID_ERROR("failed to register event handler: " <<
                       xio_strerror(xio_errno()));
        throw XioClientRegHandlerException("failed to register event handler",
                                           nullptr,
                                           xio_errno());
    }

    session = std::shared_ptr<xio_session>(xio_session_create(&params),
                                           xio_session_destroy);
    if(session == nullptr)
    {
        xio_context_del_ev_handler(ctx.get(), evfd);
        LIBLOGID_ERROR("failed to create XIO session: " <<
                       xio_strerror(xio_errno()));
        throw XioClientCreateException("failed to create XIO session",
                                       nullptr,
                                       xio_errno());
    }

    cparams.session = session.get();
    cparams.ctx = ctx.get();
    cparams.conn_user_context = this;

    conn = xio_connect(&cparams);
    if (conn == nullptr)
    {
        xio_context_del_ev_handler(ctx.get(), evfd);
        LIBLOGID_ERROR("failed to connect: " <<
                       xio_strerror(xio_errno()));
        throw XioClientCreateException("failed to connect",
                                       nullptr,
                                       xio_errno());
    }

    xio_context_run_loop(ctx.get(), 100);
    if (connect_error)
    {
        xio_context_del_ev_handler(ctx.get(), evfd);
        xio_disconnect(conn);
        xio_connection_destroy(conn);
        xio_context_run_loop(ctx.get(), XIO_INFINITE);
        LIBLOGID_ERROR("connection error: " << xio_strerror(xio_errno()));
        throw XioClientCreateException("connection error",
                                       nullptr,
                                       xio_errno());
    }

    pthread_setname_np(pthread_self(), "xio_event_loop");
    promise.set_value(true);
    xio_run_loop_worker();
}

void
NetworkXioClient::shutdown()
{
    if (not stopped)
    {
        stopping = true;
        xstop_loop();
        xio_thread_.join();
        xio_context_del_ev_handler(ctx.get(), evfd);
        while (not is_queue_empty())
        {
            xio_msg_s *req = pop_request();
            delete req;
        }
        stopped = true;
    }
}

NetworkXioClient::~NetworkXioClient()
{
    shutdown();
}

void
NetworkXioClient::xio_destroy_ctx_shutdown(xio_context *ctx)
{
    xio_context_destroy(ctx);
    xrefcnt_shutdown();
}

bool
NetworkXioClient::is_queue_empty()
{
    boost::lock_guard<decltype(inflight_lock)> lock_(inflight_lock);
    return inflight_reqs.empty();
}

NetworkXioClient::xio_msg_s*
NetworkXioClient::pop_request()
{
    boost::lock_guard<decltype(inflight_lock)> lock_(inflight_lock);
    xio_msg_s *req = inflight_reqs.front();
    inflight_reqs.pop();
    return req;
}

void
NetworkXioClient::push_request(xio_msg_s *req)
{
    boost::lock_guard<decltype(inflight_lock)> lock_(inflight_lock);
    inflight_reqs.push(req);
}

void
NetworkXioClient::xstop_loop()
{
    evfd.writefd();
}

void
NetworkXioClient::xio_run_loop_worker()
{
    while (not stopping)
    {
        int ret = xio_context_run_loop(ctx.get(), XIO_INFINITE);

        // For now we won't leave the loop if the assert is compiled out,
        // so in the worst case this will degrade into a busy loop.
        ASSERT(ret == 0);

        while (not is_queue_empty())
        {
            xio_msg_s *req = pop_request();
            ret = xio_send_request(conn, &req->xreq);
            if (ret < 0)
            {
                if (maybe_fail_request(req))
                {
                    ovs_aio_request::handle_xio_request(req->get_request(),
                                                        -1,
                                                        xio_errno(),
                                                        true);
                }
                delete req;
            }
        }
    }

    xio_disconnect(conn);
    if (not disconnected)
    {
        disconnecting = true;
        xio_context_run_loop(ctx.get(), XIO_INFINITE);
    }
    else
    {
        xio_connection_destroy(conn);
    }
}

void
NetworkXioClient::evfd_stop_loop(int /*fd*/, int /*events*/, void * /*data*/)
{
    evfd.readfd();
    xio_context_stop_loop(ctx.get());
}

int
NetworkXioClient::on_response(xio_session *session __attribute__((unused)),
                              xio_msg *reply,
                              int last_in_rxq __attribute__((unused)))
{
    NetworkXioMsg imsg;
    try
    {
        imsg.unpack_msg(static_cast<const char*>(reply->in.header.iov_base),
                        reply->in.header.iov_len);
        set_dtl_in_sync(imsg.opcode(),
                        imsg.retval(),
                        imsg.errval(),
                        imsg.dtl_in_sync());
    }
    catch (...)
    {
        LIBLOGID_ERROR("failed to unpack msg");
        return 0;
    }
    xio_msg_s *xio_msg = reinterpret_cast<xio_msg_s*>(imsg.opaque());
    if (is_ctrl_op_to_handle(imsg.opcode()))
    {
        handle_ctrl_response(imsg, xio_msg, reply);
    }
    else
    {
        insert_seen_request(xio_msg);
    }
    ovs_aio_request::handle_xio_request(xio_msg->get_request(),
                                        imsg.retval(),
                                        imsg.errval(),
                                        true);

    reply->in.header.iov_base = NULL;
    reply->in.header.iov_len = 0;
    vmsg_sglist_set_nents(&reply->in, 0);
    xio_release_response(reply);
    delete xio_msg;
    return 0;
}

int
NetworkXioClient::on_msg_error(xio_session *session __attribute__((unused)),
                               xio_status error __attribute__((unused)),
                               xio_msg_direction direction,
                               xio_msg *msg)
{
    NetworkXioMsg imsg;
    xio_msg_s *xio_msg;

    if (direction == XIO_MSG_DIRECTION_OUT)
    {
        try
        {
            imsg.unpack_msg(static_cast<const char*>(msg->out.header.iov_base),
                            msg->out.header.iov_len);
        }
        catch (...)
        {
            LIBLOGID_ERROR("failed to unpack msg");
            return 0;
        }
    }
    else /* XIO_MSG_DIRECTION_IN */
    {
        try
        {
            imsg.unpack_msg(static_cast<const char*>(msg->in.header.iov_base),
                            msg->in.header.iov_len);
            set_dtl_in_sync(imsg.opcode(),
                            imsg.retval(),
                            imsg.errval(),
                            imsg.dtl_in_sync());
        }
        catch (...)
        {
            LIBLOGID_ERROR("failed to unpack msg");
            xio_release_response(msg);
            return 0;
        }
        msg->in.header.iov_base = NULL;
        msg->in.header.iov_len = 0;
        vmsg_sglist_set_nents(&msg->in, 0);
        xio_release_response(msg);
    }
    xio_msg = reinterpret_cast<xio_msg_s*>(imsg.opaque());
    if (maybe_fail_request(xio_msg))
    {
        ovs_aio_request::handle_xio_request(xio_msg->get_request(),
                                            -1,
                                            xio_errno(),
                                            true);
    }
    delete xio_msg;
    return 0;
}

int
NetworkXioClient::on_session_event(xio_session *session __attribute__((unused)),
                                   xio_session_event_data *event_data)
{

    switch (event_data->event)
    {
    case XIO_SESSION_ERROR_EVENT:
    case XIO_SESSION_CONNECTION_ERROR_EVENT:
    case XIO_SESSION_CONNECTION_REFUSED_EVENT:
        if (event_data->reason == XIO_E_CONNECT_ERROR)
        {
            connect_error = true;
        }
        ha_ctx_.set_connection_error();
        break;
    case XIO_SESSION_CONNECTION_CLOSED_EVENT:
        break;
    case XIO_SESSION_CONNECTION_DISCONNECTED_EVENT:
        disconnected = true;
        ha_ctx_.set_connection_error();
        break;
    case XIO_SESSION_CONNECTION_TEARDOWN_EVENT:
        if (disconnecting)
        {
            xio_connection_destroy(event_data->conn);
        }
        disconnected = true;
        break;
    case XIO_SESSION_TEARDOWN_EVENT:
        xio_context_stop_loop(ctx.get());
        break;
    default:
        break;
    };
    return 0;
}

msgpack::object_handle
NetworkXioClient::msgpack_obj_handle(xio_iovec_ex *sglist)
{
    msgpack::object_handle oh =
        msgpack::unpack(reinterpret_cast<char*>(sglist[0].iov_base),
                        sglist[0].iov_len);
    return oh;
}

void
NetworkXioClient::handle_list_cluster_node_uri(xio_msg_s *xmsg,
                                               xio_iovec_ex *sglist,
                                               int vec_size)
{
    std::vector<std::string>& vec =
        *static_cast<std::vector<std::string>*>(xmsg->priv[0]);
    if (sglist[0].iov_len and sglist[0].iov_base)
    {
        uint64_t idx = 0;
        for (int i = 0; i < vec_size; i++)
        {
           vec.push_back(static_cast<char*>(sglist[0].iov_base) + idx);
           idx += strlen(static_cast<char*>(sglist[0].iov_base) + idx) + 1;
        }
    }
}

void
NetworkXioClient::handle_get_cluster_multiplier(xio_msg_s *xmsg,
                                                uint64_t cluster_size)
{
    *static_cast<uint32_t*>(xmsg->priv[0]) = cluster_size;
}

void
NetworkXioClient::handle_get_volume_uri(xio_msg_s *xio_msg,
                                        xio_iovec_ex *sglist)
{
    std::string& s = *static_cast<std::string*>(xio_msg->priv[0]);
    if (sglist[0].iov_len and sglist[0].iov_base)
    {
        s = std::string(reinterpret_cast<char*>(sglist[0].iov_base),
                        sglist[0].iov_len);
    }
}

void
NetworkXioClient::handle_get_clone_namespace_map(xio_msg_s *xio_msg,
                                                 xio_iovec_ex *sglist)
{
    CloneNamespaceMap& cn = *static_cast<CloneNamespaceMap*>(xio_msg->priv[0]);
    if (sglist[0].iov_len and sglist[0].iov_base)
    {
        msgpack::object_handle oh(msgpack_obj_handle(sglist));
        msgpack::object obj = oh.get();
        obj.convert(cn);
    }
}

void
NetworkXioClient::handle_get_page_vector(xio_msg_s *xio_msg,
                                         xio_iovec_ex *sglist)
{
    ClusterLocationPage& cl =
        *static_cast<ClusterLocationPage*>(xio_msg->priv[0]);
    if (sglist[0].iov_len and sglist[0].iov_base)
    {
        msgpack::object_handle oh(msgpack_obj_handle(sglist));
        msgpack::object obj = oh.get();
        obj.convert(cl);
    }
}

void
NetworkXioClient::handle_ctrl_response(const NetworkXioMsg& imsg,
                                       xio_msg_s *msg,
                                       xio_msg *reply)
{
    switch (imsg.opcode())
    {
    case NetworkXioMsgOpcode::GetVolumeURIRsp:
        handle_get_volume_uri(msg,
                              vmsg_sglist(&reply->in));
        break;
    case NetworkXioMsgOpcode::ListClusterNodeURIRsp:
        handle_list_cluster_node_uri(msg,
                                     vmsg_sglist(&reply->in), imsg.size());
        break;
    case NetworkXioMsgOpcode::GetClusterMultiplierRsp:
        handle_get_cluster_multiplier(msg,
                                      imsg.u64());
        break;
    case NetworkXioMsgOpcode::GetPageRsp:
        handle_get_page_vector(msg,
                               vmsg_sglist(&reply->in));
        break;
    case NetworkXioMsgOpcode::StatVolumeRsp:
        handle_stat(msg, imsg.u64());
        break;
    case NetworkXioMsgOpcode::GetCloneNamespaceMapRsp:
        handle_get_clone_namespace_map(msg,
                                       vmsg_sglist(&reply->in));
        break;
    case NetworkXioMsgOpcode::ListVolumesRsp:
        handle_list_volumes(msg,
                            vmsg_sglist(&reply->in),
                            imsg.retval());
        break;
    case NetworkXioMsgOpcode::ListSnapshotsRsp:
        handle_list_snapshots(msg,
                              vmsg_sglist(&reply->in),
                              imsg.retval(),
                              imsg.size());
        break;
    case NetworkXioMsgOpcode::OpenRsp:
    case NetworkXioMsgOpcode::CloseRsp:
    case NetworkXioMsgOpcode::CreateVolumeRsp:
    case NetworkXioMsgOpcode::RemoveVolumeRsp:
    case NetworkXioMsgOpcode::TruncateRsp:
    case NetworkXioMsgOpcode::CreateSnapshotRsp:
    case NetworkXioMsgOpcode::RollbackSnapshotRsp:
    case NetworkXioMsgOpcode::IsSnapshotSyncedRsp:
    case NetworkXioMsgOpcode::DeleteSnapshotRsp:
        break;
    default:
        LIBLOGID_ERROR("unhandled control operation (data path)");
        abort();
        break;
    }
}

void
NetworkXioClient::xio_send_open_request(const std::string& volname,
                                        ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::OpenReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volname);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_send_read_request(void *buf,
                                        const uint64_t size_in_bytes,
                                        const uint64_t offset_in_bytes,
                                        ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::ReadReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.size(size_in_bytes);
    xmsg->msg.offset(offset_in_bytes);

    xio_msg_prepare(xmsg);

    vmsg_sglist_set_nents(&xmsg->xreq.in, 1);
    xmsg->xreq.in.data_iov.sglist[0].iov_base = buf;
    xmsg->xreq.in.data_iov.sglist[0].iov_len = size_in_bytes;
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_send_write_request(const void *buf,
                                         const uint64_t size_in_bytes,
                                         const uint64_t offset_in_bytes,
                                         ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::WriteReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.size(size_in_bytes);
    xmsg->msg.offset(offset_in_bytes);

    xio_msg_prepare(xmsg);

    vmsg_sglist_set_nents(&xmsg->xreq.out, 1);
    xmsg->xreq.out.data_iov.sglist[0].iov_base = const_cast<void*>(buf);
    xmsg->xreq.out.data_iov.sglist[0].iov_len = size_in_bytes;
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_send_flush_request(ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::FlushReq);
    xmsg->msg.opaque((uintptr_t)xmsg);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_send_close_request(ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::CloseReq);
    xmsg->msg.opaque((uintptr_t)xmsg);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_get_volume_uri(const char *volume_name,
                                     std::string& volume_uri,
                                     ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->priv[0] = static_cast<void*>(&volume_uri);
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::GetVolumeURIReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_list_cluster_node_uri(std::vector<std::string>& uris,
                                            ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->priv[0] = static_cast<void*>(&uris);
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::ListClusterNodeURIReq);
    xmsg->msg.opaque((uintptr_t)xmsg);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_get_cluster_multiplier(const char *volume_name,
                                             uint32_t *cluster_multiplier,
                                             ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->priv[0] = static_cast<void*>(cluster_multiplier);
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::GetClusterMultiplierReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_get_page(const char *volume_name,
                               const ClusterAddress ca,
                               ClusterLocationPage& cl,
                               ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->priv[0] = static_cast<void*>(&cl);
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::GetPageReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);
    xmsg->msg.u64(static_cast<uint64_t>(ca));

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_get_clone_namespace_map(const char *volume_name,
                                              CloneNamespaceMap& cn,
                                              ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->priv[0] = static_cast<void*>(&cn);
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::GetCloneNamespaceMapReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_create_volume(const char *volume_name,
                                    size_t size,
                                    ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::CreateVolumeReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);
    xmsg->msg.size(size);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_remove_volume(const char *volume_name,
                                    ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::RemoveVolumeReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_truncate_volume(const char *volume_name,
                                      uint64_t offset,
                                      ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::TruncateReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);
    xmsg->msg.offset(offset);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_stat_volume(const std::string& volume_name,
                                  uint64_t *size,
                                  ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->priv[0] = static_cast<void*>(size);
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::StatVolumeReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_list_volumes(std::vector<std::string>& volumes,
                                   ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->priv[0] = static_cast<void*>(&volumes);
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::ListVolumesReq);
    xmsg->msg.opaque((uintptr_t)xmsg);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_list_snapshots(const char *volume_name,
                                     std::vector<std::string>& snapshots,
                                     uint64_t *size,
                                     ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->priv[0] = static_cast<void*>(&snapshots);
    xmsg->priv[1] = static_cast<void*>(size);
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::ListSnapshotsReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_create_snapshot(const char *volume_name,
                                      const char *snap_name,
                                      int64_t timeout,
                                      ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::CreateSnapshotReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);
    xmsg->msg.snap_name(snap_name);
    xmsg->msg.timeout(timeout);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_delete_snapshot(const char *volume_name,
                                      const char *snap_name,
                                      ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::DeleteSnapshotReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);
    xmsg->msg.snap_name(snap_name);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_rollback_snapshot(const char *volume_name,
                                        const char *snap_name,
                                        ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::RollbackSnapshotReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);
    xmsg->msg.snap_name(snap_name);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::xio_is_snapshot_synced(const char *volume_name,
                                         const char *snap_name,
                                         ovs_aio_request *request)
{
    xio_msg_s *xmsg = new xio_msg_s;
    xmsg->set_opaque(request);
    xmsg->msg.opcode(NetworkXioMsgOpcode::IsSnapshotSyncedReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volume_name);
    xmsg->msg.snap_name(snap_name);

    xio_msg_prepare(xmsg);
    push_request(xmsg);
    xstop_loop();
}

void
NetworkXioClient::create_vec_from_buf(xio_msg_s *xmsg,
                                      xio_iovec_ex *sglist,
                                      int vec_size)
{
    std::vector<std::string>& vec =
        *static_cast<std::vector<std::string>*>(xmsg->priv[0]);
    if (sglist[0].iov_len and sglist[0].iov_base)
    {
        uint64_t idx = 0;
        for (int i = 0; i < vec_size; i++)
        {
           vec.push_back(static_cast<char*>(sglist[0].iov_base) + idx);
           idx += strlen(static_cast<char*>(sglist[0].iov_base) + idx) + 1;
        }
    }
}

void
NetworkXioClient::handle_list_volumes(xio_msg_s *xmsg,
                                      xio_iovec_ex *sglist,
                                      int vec_size)
{
    create_vec_from_buf(xmsg, sglist, vec_size);
}

void
NetworkXioClient::handle_list_snapshots(xio_msg_s *xmsg,
                                        xio_iovec_ex *sglist,
                                        int vec_size,
                                        size_t volsize)
{
    create_vec_from_buf(xmsg, sglist, vec_size);
    uint64_t *size = static_cast<uint64_t*>(xmsg->priv[1]);
    *size = volsize;
}

void
NetworkXioClient::handle_stat(xio_msg_s *xio_msg,
                              uint64_t volsize)
{
    uint64_t *size = static_cast<uint64_t*>(xio_msg->priv[0]);
    *size = volsize;
}

void
NetworkXioClient::xio_msg_prepare(xio_msg_s *xmsg)
{
    xmsg->s_msg = xmsg->msg.pack_msg();

    memset(static_cast<void*>(&xmsg->xreq), 0, sizeof(xio_msg));

    vmsg_sglist_set_nents(&xmsg->xreq.out, 0);
    xmsg->xreq.out.header.iov_base = (void*)xmsg->s_msg.c_str();
    xmsg->xreq.out.header.iov_len = xmsg->s_msg.length();
}

} //namespace libovsvolumedriver
