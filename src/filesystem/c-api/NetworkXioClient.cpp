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
#include "NetworkXioClient.h"

#include <youtils/Assert.h>

#include <thread>
#include <future>
#include <functional>
#include <atomic>
#include <system_error>

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
                                   NetworkHAContext& ha_ctx,
                                   bool ha_try_reconnect)
    : uri_(uri)
    , stopping(false)
    , stopped(false)
    , disconnected(false)
    , disconnecting(false)
    , nr_req_queue(qd)
    , evfd()
    , ha_ctx_(ha_ctx)
    , ha_try_reconnect_(ha_try_reconnect)
    , connection_error_(false)
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
    catch (const std::exception& e)
    {
        LIBLOGID_ERROR("failed to create xio thread: " << e.what());
        xio_thread_.join();
        throw XioClientCreateException("failed to create XIO worker thread");
    }
}

void
NetworkXioClient::run(std::promise<bool>& promise)
{
    int xopt = 0;
    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_ENABLE_FLOW_CONTROL,
                &xopt, sizeof(int));

    xopt = 2 * nr_req_queue;
    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO, XIO_OPTNAME_SND_QUEUE_DEPTH_MSGS,
                &xopt, sizeof(int));

    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO, XIO_OPTNAME_RCV_QUEUE_DEPTH_MSGS,
                &xopt, sizeof(int));

    struct xio_options_keepalive ka;
    ka.time =
        yt::System::get_env_with_default<int>("LIBOVSVOLUMEDRIVER_XIO_KEEPALIVE_TIME",
                                              600);
    ka.intvl =
        yt::System::get_env_with_default<int>("LIBOVSVOLUMEDRIVER_XIO_KEEPALIVE_INTVL",
                                              60);
    ka.probes =
        yt::System::get_env_with_default<int>("LIBOVSVOLUMEDRIVER_XIO_KEEPALIVE_PROBES",
                                              20);

    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_CONFIG_KEEPALIVE,
                &ka,
                sizeof(ka));
    try
    {
        int polling_timeout_us =
        yt::System::get_env_with_default<int>("LIBOVSVOLUMEDRIVER_XIO_POLLING_TIMEOUT_US",
                                              0);
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
        LIBLOGID_ERROR("failed to create XIO context");
        throw XioClientCreateException("failed to create XIO context");
    }

    if(xio_context_add_ev_handler(ctx.get(),
                                  evfd,
                                  XIO_POLLIN,
                                  static_evfd_stop_loop<NetworkXioClient>,
                                  this))
    {
        LIBLOGID_ERROR("failed to register event handler");
        throw XioClientRegHandlerException("failed to register event handler");
    }

    session = std::shared_ptr<xio_session>(xio_session_create(&params),
                                           xio_session_destroy);
    if(session == nullptr)
    {
        xio_context_del_ev_handler(ctx.get(), evfd);
        LIBLOGID_ERROR("failed to create XIO session");
        throw XioClientCreateException("failed to create XIO session");
    }

    cparams.session = session.get();
    cparams.ctx = ctx.get();
    cparams.conn_user_context = this;

    conn = xio_connect(&cparams);
    if (conn == nullptr)
    {
        xio_context_del_ev_handler(ctx.get(), evfd);
        LIBLOGID_ERROR("failed to connect");
        throw XioClientCreateException("failed to connect");
    }

    pthread_setname_np(pthread_self(), "xio_loop_worker");
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
                req_queue_release();
                if ((not is_ha_enabled()) or try_fail_request_on_conn_error())
                {
                    ovs_aio_request::handle_xio_request(req->get_request(),
                                                        -1,
                                                        EIO,
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

    insert_seen_request(xio_msg);
    ovs_aio_request::handle_xio_request(xio_msg->get_request(),
                                        imsg.retval(),
                                        imsg.errval(),
                                        true);

    reply->in.header.iov_base = NULL;
    reply->in.header.iov_len = 0;
    vmsg_sglist_set_nents(&reply->in, 0);
    xio_release_response(reply);
    req_queue_release();
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
    if (try_fail_request_on_msg_error())
    {
        ovs_aio_request::handle_xio_request(xio_msg->get_request(),
                                            -1,
                                            EIO,
                                            true);
    }
    req_queue_release();
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
        connection_error_ = true;
        ha_ctx_.set_connection_error();
        break;
    case XIO_SESSION_CONNECTION_CLOSED_EVENT:
        break;
    case XIO_SESSION_CONNECTION_DISCONNECTED_EVENT:
        connection_error_ = disconnected = true;
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

void
NetworkXioClient::req_queue_wait_until(xio_msg_s *xmsg)
{
    using namespace std::chrono_literals;
    std::unique_lock<std::mutex> l_(req_queue_lock);
    if (--nr_req_queue <= 0)
    {
        TODO("cnanakos: export cv timeout")
        if (not req_queue_cond.wait_until(l_,
                                          std::chrono::steady_clock::now() +
                                          60s,
                                          [&]{return nr_req_queue >= 0;}))
        {
            delete xmsg;
            LIBLOGID_ERROR("request queue is busy");
            throw XioClientQueueIsBusyException("request queue is busy");
        }
    }
}

void
NetworkXioClient::req_queue_release()
{
    std::lock_guard<std::mutex> l_(req_queue_lock);
    nr_req_queue++;
    req_queue_cond.notify_one();
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
    req_queue_wait_until(xmsg);
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
    req_queue_wait_until(xmsg);
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
    req_queue_wait_until(xmsg);
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
    req_queue_wait_until(xmsg);
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
    req_queue_wait_until(xmsg);
    push_request(xmsg);
    xstop_loop();
}

int
NetworkXioClient::on_msg_error_control(xio_session *session ATTR_UNUSED,
                                       xio_status error ATTR_UNUSED,
                                       xio_msg_direction direction,
                                       xio_msg *msg,
                                       void *cb_user_context)
{
    NetworkXioMsg imsg;
    xio_msg_s *xio_msg;

    session_data *sdata = static_cast<session_data*>(cb_user_context);
    xio_context *ctx = sdata->ctx;
    if (direction == XIO_MSG_DIRECTION_IN)
    {
        try
        {
            imsg.unpack_msg(static_cast<const char*>(msg->in.header.iov_base),
                            msg->in.header.iov_len);
        }
        catch (...)
        {
            xio_release_response(msg);
            return 0;
        }
        msg->in.header.iov_base = NULL;
        msg->in.header.iov_len = 0;
        vmsg_sglist_set_nents(&msg->in, 0);
        xio_release_response(msg);
    }
    else /* XIO_MSG_DIRECTION_OUT */
    {
        try
        {
            imsg.unpack_msg(static_cast<const char*>(msg->out.header.iov_base),
                            msg->out.header.iov_len);
        }
        catch (...)
        {
            LIBLOG_ERROR("failed to unpack msg");
            return 0;
        }
    }
    xio_msg = reinterpret_cast<xio_msg_s*>(imsg.opaque());
    ovs_aio_request::handle_xio_ctrl_request(xio_msg->get_request(),
                                             -1,
                                             sdata->connection_error ?
                                             ENOTCONN : EIO);
    xio_context_stop_loop(ctx);
    return 0;
}

int
NetworkXioClient::on_msg_control(xio_session *session ATTR_UNUSED,
                                 xio_msg *reply,
                                 int last_in_rxq ATTR_UNUSED,
                                 void *cb_user_context)
{
    session_data *sdata = static_cast<session_data*>(cb_user_context);
    xio_context *ctx = sdata->ctx;
    NetworkXioMsg imsg;
    try
    {
        imsg.unpack_msg(static_cast<const char*>(reply->in.header.iov_base),
                        reply->in.header.iov_len);
    }
    catch (...)
    {
        LIBLOG_ERROR("failed to unpack msg");
        return 0;
    }
    xio_ctl_s *xctl = reinterpret_cast<xio_ctl_s*>(imsg.opaque());
    ovs_aio_request::handle_xio_ctrl_request(xctl->xmsg.get_request(),
                                             imsg.retval(),
                                             imsg.errval());

    assert(vmsg_sglist(&reply->in));
    switch (imsg.opcode())
    {
    case NetworkXioMsgOpcode::ListVolumesRsp:
        handle_list_volumes(xctl,
                            vmsg_sglist(&reply->in),
                            imsg.retval());
        break;
    case NetworkXioMsgOpcode::ListSnapshotsRsp:
        handle_list_snapshots(xctl,
                              vmsg_sglist(&reply->in),
                              imsg.retval(),
                              imsg.size());
        break;
    case NetworkXioMsgOpcode::ListClusterNodeURIRsp:
        handle_list_cluster_node_uri(xctl,
                                     vmsg_sglist(&reply->in),
                                     imsg.size());
        break;
    case NetworkXioMsgOpcode::GetVolumeURIRsp:
        handle_get_volume_uri(xctl,
                              vmsg_sglist(&reply->in),
                              imsg.size());
        break;
    default:
        break;
    }
    reply->in.header.iov_base = NULL;
    reply->in.header.iov_len = 0;
    vmsg_sglist_set_nents(&reply->in, 0);
    xio_release_response(reply);
    xio_context_stop_loop(ctx);
    return 0;
}

int
NetworkXioClient::on_session_event_control(xio_session *session,
                                           xio_session_event_data *event_data,
                                           void *cb_user_context)
{
    session_data *sdata = static_cast<session_data*>(cb_user_context);
    xio_context *ctx = sdata->ctx;
    switch (event_data->event)
    {
    case XIO_SESSION_CONNECTION_TEARDOWN_EVENT:
        if (sdata->disconnecting)
        {
            xio_connection_destroy(event_data->conn);
        }
        sdata->disconnected = true;
        xio_context_stop_loop(ctx);
        break;
    case XIO_SESSION_CONNECTION_ERROR_EVENT:
    case XIO_SESSION_CONNECTION_REFUSED_EVENT:
    case XIO_SESSION_CONNECTION_DISCONNECTED_EVENT:
        sdata->connection_error = true;
        break;
    case XIO_SESSION_TEARDOWN_EVENT:
        xio_session_destroy(session);
        xio_context_stop_loop(ctx);
        break;
    default:
        break;
    }
    return 0;
}

xio_connection*
NetworkXioClient::create_connection_control(session_data *sdata,
                                            const std::string& uri)
{
    xio_connection *conn;
    xio_session *session;
    xio_session_params params;
    xio_connection_params cparams;

    xio_session_ops s_ops;
    s_ops.on_session_event = on_session_event_control;
    s_ops.on_session_established = NULL;
    s_ops.on_msg = on_msg_control;
    s_ops.on_msg_error = on_msg_error_control;
    s_ops.assign_data_in_buf = NULL;

    memset(&params, 0, sizeof(params));
    params.type = XIO_SESSION_CLIENT;
    params.ses_ops = &s_ops;
    params.uri = uri.c_str();
    params.user_context = sdata;

    session = xio_session_create(&params);
    if (not session)
    {
        return nullptr;
    }
    memset(&cparams, 0, sizeof(cparams));
    cparams.session = session;
    cparams.ctx = sdata->ctx;
    cparams.conn_user_context = sdata;

    conn = xio_connect(&cparams);
    return conn;
}

void
NetworkXioClient::xio_submit_request(const std::string& uri,
                                     xio_ctl_s *xctl,
                                     ovs_aio_request *request)
{
    xrefcnt_init();

    auto ctx = std::shared_ptr<xio_context>(xio_context_create(NULL,
                                                               0,
                                                               -1),
                                            xio_destroy_ctx_shutdown);
    xctl->sdata.ctx = ctx.get();
    xctl->sdata.disconnecting = false;
    xctl->sdata.disconnected = false;
    xio_connection *conn = create_connection_control(&xctl->sdata, uri);
    if (conn == nullptr)
    {
        ovs_aio_request::handle_xio_ctrl_request(request,
                                                 -1,
                                                 EIO);
        return;
    }

    int ret = xio_send_request(conn, &xctl->xmsg.xreq);
    if (ret < 0)
    {
        ovs_aio_request::handle_xio_ctrl_request(request,
                                                 -1,
                                                 EIO);
        goto exit;
    }
    xio_context_run_loop(ctx.get(), XIO_INFINITE);
exit:
    xio_disconnect(conn);
    if (not xctl->sdata.disconnected)
    {
        xctl->sdata.disconnecting = true;
    }
    else
    {
        xio_connection_destroy(conn);
    }
    xio_context_run_loop(ctx.get(), XIO_INFINITE);
}

void
NetworkXioClient::create_vec_from_buf(xio_ctl_s *xctl,
                                      xio_iovec_ex *sglist,
                                      int vec_size)
{
    uint64_t idx = 0;
    for (int i = 0; i < vec_size; i++)
    {
       assert(sglist);
       xctl->vec.push_back(static_cast<char*>(sglist[0].iov_base) + idx);
       idx += strlen(static_cast<char*>(sglist[0].iov_base) + idx) + 1;
    }
}

void
NetworkXioClient::handle_list_volumes(xio_ctl_s *xctl,
                                      xio_iovec_ex *sglist,
                                      int vec_size)
{
    create_vec_from_buf(xctl, sglist, vec_size);
}

void
NetworkXioClient::handle_list_snapshots(xio_ctl_s *xctl,
                                        xio_iovec_ex *sglist,
                                        int vec_size,
                                        int size)
{
    create_vec_from_buf(xctl, sglist, vec_size);
    xctl->size = size;
}

void
NetworkXioClient::handle_list_cluster_node_uri(xio_ctl_s *xctl,
                                               xio_iovec_ex *sglist,
                                               int vec_size)
{
    create_vec_from_buf(xctl, sglist, vec_size);
}

void
NetworkXioClient::handle_get_volume_uri(xio_ctl_s *xctl,
                                        xio_iovec_ex *sglist,
                                        int size)
{
    try
    {
        auto data = std::make_unique<uint8_t[]>(size);
        memcpy(data.get(), sglist[0].iov_base, size);
        xctl->data = std::move(data);
        xctl->size = size;
    }
    catch (const std::bad_alloc&)
    {}
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

void
NetworkXioClient::xio_create_volume(const std::string& uri,
                                    const char* volume_name,
                                    size_t size,
                                    ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::CreateVolumeReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);
    xctl->xmsg.msg.size(size);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
}

void
NetworkXioClient::xio_truncate_volume(const std::string& uri,
                                      const char* volume_name,
                                     uint64_t offset,
                                     ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::TruncateReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);
    xctl->xmsg.msg.offset(offset);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
}

void
NetworkXioClient::xio_remove_volume(const std::string& uri,
                                    const char* volume_name,
                                    ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::RemoveVolumeReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
}

void
NetworkXioClient::xio_stat_volume(const std::string& uri,
                                  const std::string& volume_name,
                                  ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::StatVolumeReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
}

void
NetworkXioClient::xio_list_volumes(const std::string& uri,
                                   std::vector<std::string>& volumes,
                                   ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::ListVolumesReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
    xctl->vec.swap(volumes);
}

void
NetworkXioClient::xio_list_cluster_node_uri(const std::string& uri,
                                            std::vector<std::string>& uris,
                                            ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::ListClusterNodeURIReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
    xctl->vec.swap(uris);
}

void
NetworkXioClient::xio_get_volume_uri(const std::string& uri,
                                     const char* volume_name,
                                     std::string& volume_uri,
                                     ovs_aio_request *request)
{
    auto xctl(std::make_unique<xio_ctl_s>());
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::GetVolumeURIReq);
    xctl->xmsg.msg.opaque(reinterpret_cast<uintptr_t>(xctl.get()));
    xctl->xmsg.msg.volume_name(volume_name);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);

    if (xctl->data)
    {
        volume_uri = std::string(reinterpret_cast<char*>(xctl->data.get()),
                                 xctl->size);
    }
}

void
NetworkXioClient::xio_list_snapshots(const std::string& uri,
                                     const char* volume_name,
                                     std::vector<std::string>& snapshots,
                                     uint64_t *size,
                                     ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::ListSnapshotsReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
    xctl->vec.swap(snapshots);
    *size = xctl->size;
}

void
NetworkXioClient::xio_create_snapshot(const std::string& uri,
                                      const char* volume_name,
                                      const char* snap_name,
                                      int64_t timeout,
                                      ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::CreateSnapshotReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);
    xctl->xmsg.msg.snap_name(snap_name);
    xctl->xmsg.msg.timeout(timeout);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
}

void
NetworkXioClient::xio_delete_snapshot(const std::string& uri,
                                      const char* volume_name,
                                      const char* snap_name,
                                      ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::DeleteSnapshotReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);
    xctl->xmsg.msg.snap_name(snap_name);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
}

void
NetworkXioClient::xio_rollback_snapshot(const std::string& uri,
                                        const char* volume_name,
                                        const char* snap_name,
                                        ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::RollbackSnapshotReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);
    xctl->xmsg.msg.snap_name(snap_name);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
}

void
NetworkXioClient::xio_is_snapshot_synced(const std::string& uri,
                                         const char* volume_name,
                                         const char* snap_name,
                                         ovs_aio_request *request)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.set_opaque(request);
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::IsSnapshotSyncedReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);
    xctl->xmsg.msg.snap_name(snap_name);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), request);
}

} //namespace libovsvolumedriver
