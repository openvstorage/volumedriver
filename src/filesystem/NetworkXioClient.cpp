// Copyright 2016 iNuron NV
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

#include <thread>
#include <functional>
#include <atomic>
#include <system_error>

#include <youtils/Assert.h>

#include "NetworkXioClient.h"

#define POLLING_TIME_USEC   20

std::atomic<int> xio_init_refcnt =  ATOMIC_VAR_INIT(0);

namespace volumedriverfs
{

MAKE_EXCEPTION(FailedCreateXioClient, fungi::IOException);

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
static int
static_assign_data_in_buf(xio_msg *msg,
                          void *cb_user_context)
{
    T *obj = reinterpret_cast<T*>(cb_user_context);
    if (obj == NULL)
    {
        assert(obj != NULL);
        return -1;
    }
    return obj->assign_data_in_buf(msg);
}

NetworkXioClient::NetworkXioClient(const std::string& uri,
                                   const size_t msg_q_depth)
    : uri_(uri)
    , queue_(msg_q_depth)
    , stopping(false)
    , disconnected(false)
{
    ses_ops.on_session_event = static_on_session_event<NetworkXioClient>;
    ses_ops.on_session_established = NULL;
    ses_ops.on_msg = static_on_response<NetworkXioClient>;
    ses_ops.on_msg_error = static_on_msg_error<NetworkXioClient>;
    ses_ops.on_cancel_request = NULL;
    ses_ops.assign_data_in_buf = static_assign_data_in_buf<NetworkXioClient>;

    memset(&params, 0, sizeof(params));
    memset(&cparams, 0, sizeof(cparams));

    params.type = XIO_SESSION_CLIENT;
    params.ses_ops = &ses_ops;
    params.user_context = this;
    params.uri = uri_.c_str();

    if (xio_init_refcnt.fetch_add(1, std::memory_order_relaxed) == 0)
    {
        xio_init();
    }

    int xopt = 0;
    int queue_depth = 2048;
    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_ENABLE_FLOW_CONTROL,
                &xopt, sizeof(int));

    xopt = queue_depth;
    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO, XIO_OPTNAME_SND_QUEUE_DEPTH_MSGS,
                &xopt, sizeof(int));

    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO, XIO_OPTNAME_RCV_QUEUE_DEPTH_MSGS,
                &xopt, sizeof(int));

    ctx = xio_context_create(NULL, POLLING_TIME_USEC, -1);
    if (ctx == NULL)
    {
        goto err_exit2;
    }

    session = xio_session_create(&params);

    cparams.session = session;
    cparams.ctx = ctx;
    cparams.conn_user_context = this;

    conn = xio_connect(&cparams);
    if (conn == NULL)
    {
        goto err_exit1;
    }

    try
    {
        xio_thread_ = std::thread([&](){
                    auto fp = std::bind(&NetworkXioClient::xio_run_loop_worker,
                                        this,
                                        std::placeholders::_1);
                    pthread_setname_np(pthread_self(), "xio_run_loop_worker");
                    fp(this);
                });
    }
    catch (const std::system_error&)
    {
        goto err_exit1;
    }
    return;

err_exit1:
    xio_session_destroy(session);
    xio_context_destroy(ctx);
err_exit2:
    throw FailedCreateXioClient("failed to create XIO client");
}

NetworkXioClient::~NetworkXioClient()
{
    stopping = true;
    xio_context_stop_loop(ctx);
    xio_thread_.join();
    while (not queue_.empty())
    {
        xio_msg_s *req;
        queue_.pop(req);
        delete req;
    }
    if (xio_init_refcnt.fetch_sub(1, std::memory_order_relaxed) == 1)
    {
        xio_shutdown();
    }
}

void
NetworkXioClient::xio_run_loop_worker(void *arg)
{
    NetworkXioClient *cli = reinterpret_cast<NetworkXioClient*>(arg);
    while (not stopping)
    {
        int ret = xio_context_run_loop(cli->ctx, XIO_INFINITE);
        ASSERT(ret == 0);
        while (not cli->queue_.empty())
        {
            xio_msg_s *req;
            cli->queue_.pop(req);
            int r = xio_send_request(cli->conn, &req->xreq);
            if (r < 0)
            {
                int saved_errno = xio_errno();
                if (saved_errno == XIO_E_TX_QUEUE_OVERFLOW)
                {
                    /* not supported yet - flow control is disabled */
                    ovs_xio_aio_complete_request(const_cast<void*>(req->opaque),
                                                 -1,
                                                 EBUSY);
                }
                else
                {
                    /* fail request with EIO in any other case */
                    ovs_xio_aio_complete_request(const_cast<void*>(req->opaque),
                                                 -1,
                                                 EIO);
                }
                delete req;
            }
        }
    }

    xio_disconnect(cli->conn);
    if (not disconnected)
    {
        xio_context_run_loop(cli->ctx, XIO_INFINITE);
        xio_session_destroy(cli->session);
    }
    else
    {
        xio_session_destroy(cli->session);
    }
    xio_context_destroy(cli->ctx);
    return;
}

int
NetworkXioClient::assign_data_in_buf(xio_msg *msg)
{
    xio_iovec_ex *sglist = vmsg_sglist(&msg->in);
    xio_reg_mem xbuf;

    if (!sglist[0].iov_len)
    {
        return 0;
    }

    xio_mem_alloc(sglist[0].iov_len, &xbuf);

    sglist[0].iov_base = xbuf.addr;
    sglist[0].mr = xbuf.mr;
    return 0;
}

int
NetworkXioClient::on_msg_error(xio_session *session __attribute__((unused)),
                               xio_status error __attribute__((unused)),
                               xio_msg_direction direction,
                               xio_msg *msg)
{
    if (direction == XIO_MSG_DIRECTION_OUT)
    {
        NetworkXioMsg i_msg;
        try
        {
            i_msg.unpack_msg(static_cast<const char*>(msg->out.header.iov_base),
                             msg->out.header.iov_len);
        }
        catch (...)
        {
            //cnanakos: client logging?
            return 0;
        }
        xio_msg_s *xio_msg = reinterpret_cast<xio_msg_s*>(i_msg.opaque());
        ovs_xio_aio_complete_request(const_cast<void*>(xio_msg->opaque),
                                     -1,
                                     EIO);
        delete xio_msg;
    }
    else /* XIO_MSG_DIRECTION_IN */
    {
        xio_release_response(msg);
    }
    return 0;
}

int
NetworkXioClient::on_session_event(xio_session *session __attribute__((unused)),
                                   xio_session_event_data *event_data)
{

    switch (event_data->event)
    {
    case XIO_SESSION_CONNECTION_TEARDOWN_EVENT:
        xio_connection_destroy(event_data->conn);
        disconnected = true;
        break;
    case XIO_SESSION_TEARDOWN_EVENT:
        xio_context_stop_loop(ctx);
        break;
    default:
        break;
    };
    return 0;
}

int
NetworkXioClient::xio_send_open_request(const std::string& volname,
                                        const void *opaque)
{
    xio_msg_s *xmsg;
    try{ xmsg = new xio_msg_s; }catch(const std::bad_alloc&) { return -1; };
    xmsg->opaque = opaque;
    xmsg->msg.opcode(NetworkXioMsgOpcode::OpenReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.volume_name(volname);

    xmsg->s_msg = xmsg->msg.pack_msg();

    memset(static_cast<void*>(&xmsg->xreq), 0, sizeof(xio_msg));

    vmsg_sglist_set_nents(&xmsg->xreq.out, 0);
    xmsg->xreq.out.header.iov_base = (void*)xmsg->s_msg.c_str();
    xmsg->xreq.out.header.iov_len = xmsg->s_msg.length();
    queue_.push(xmsg);
    xio_context_stop_loop(ctx);
    return 0;
}

int
NetworkXioClient::xio_send_read_request(void *buf,
                                        const uint64_t size_in_bytes,
                                        const uint64_t offset_in_bytes,
                                        const void *opaque)
{
    xio_msg_s *xmsg;
    try{ xmsg = new xio_msg_s; }catch(const std::bad_alloc&) { return -1; };
    xmsg->opaque = opaque;
    xmsg->msg.opcode(NetworkXioMsgOpcode::ReadReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.size(size_in_bytes);
    xmsg->msg.offset(offset_in_bytes);

    xmsg->s_msg = xmsg->msg.pack_msg();

    memset(static_cast<void*>(&xmsg->xreq), 0, sizeof(xio_msg));

    vmsg_sglist_set_nents(&xmsg->xreq.out, 0);
    xmsg->xreq.out.header.iov_base = (void*)xmsg->s_msg.c_str();
    xmsg->xreq.out.header.iov_len = xmsg->s_msg.length();

    vmsg_sglist_set_nents(&xmsg->xreq.in, 1);
    xmsg->xreq.in.data_iov.sglist[0].iov_base = buf;
    xmsg->xreq.in.data_iov.sglist[0].iov_len = size_in_bytes;
    queue_.push(xmsg);
    xio_context_stop_loop(ctx);
    return 0;
}

int
NetworkXioClient::xio_send_write_request(const void *buf,
                                         const uint64_t size_in_bytes,
                                         const uint64_t offset_in_bytes,
                                         const void *opaque)
{
    xio_msg_s *xmsg;
    try{ xmsg = new xio_msg_s; }catch(const std::bad_alloc&) { return -1; };
    xmsg->opaque = opaque;
    xmsg->msg.opcode(NetworkXioMsgOpcode::WriteReq);
    xmsg->msg.opaque((uintptr_t)xmsg);
    xmsg->msg.size(size_in_bytes);
    xmsg->msg.offset(offset_in_bytes);

    xmsg->s_msg = xmsg->msg.pack_msg();

    memset(static_cast<void*>(&xmsg->xreq), 0, sizeof(xio_msg));

    vmsg_sglist_set_nents(&xmsg->xreq.out, 1);
    xmsg->xreq.out.header.iov_base = (void*)xmsg->s_msg.c_str();
    xmsg->xreq.out.header.iov_len = xmsg->s_msg.length();
    xmsg->xreq.out.data_iov.sglist[0].iov_base = const_cast<void*>(buf);
    xmsg->xreq.out.data_iov.sglist[0].iov_len = size_in_bytes;
    queue_.push(xmsg);
    xio_context_stop_loop(ctx);
    return 0;
}

int
NetworkXioClient::xio_send_close_request(const void *opaque)
{
    xio_msg_s *xmsg;
    try{ xmsg = new xio_msg_s; }catch(const std::bad_alloc&) { return -1; };
    xmsg->opaque = opaque;
    xmsg->msg.opcode(NetworkXioMsgOpcode::CloseReq);
    xmsg->msg.opaque((uintptr_t)xmsg);

    xmsg->s_msg = xmsg->msg.pack_msg();

    memset(static_cast<void*>(&xmsg->xreq), 0, sizeof(xio_msg));

    vmsg_sglist_set_nents(&xmsg->xreq.out, 0);
    xmsg->xreq.out.header.iov_base = (void*)xmsg->s_msg.c_str();
    xmsg->xreq.out.header.iov_len = xmsg->s_msg.length();
    queue_.push(xmsg);
    xio_context_stop_loop(ctx);
    return 0;
}

int
NetworkXioClient::xio_send_flush_request(const void *opaque)
{
    xio_msg_s *xmsg;
    try{ xmsg = new xio_msg_s; }catch(const std::bad_alloc&) { return -1; };
    xmsg->opaque = opaque;
    xmsg->msg.opcode(NetworkXioMsgOpcode::FlushReq);
    xmsg->msg.opaque((uintptr_t)xmsg);

    xmsg->s_msg = xmsg->msg.pack_msg();

    memset(static_cast<void*>(&xmsg->xreq), 0, sizeof(xio_msg));

    vmsg_sglist_set_nents(&xmsg->xreq.out, 0);
    xmsg->xreq.out.header.iov_base = (void*)xmsg->s_msg.c_str();
    xmsg->xreq.out.header.iov_len = xmsg->s_msg.length();
    queue_.push(xmsg);
    xio_context_stop_loop(ctx);
    return 0;
}

int
NetworkXioClient::on_response(xio_session *session __attribute__((unused)),
                              xio_msg *reply,
                              int last_in_rxq __attribute__((unused)))
{
    NetworkXioMsg i_msg;
    try
    {
        i_msg.unpack_msg(static_cast<const char*>(reply->in.header.iov_base),
                         reply->in.header.iov_len);
    }
    catch (...)
    {
        //cnanakos: logging
        return 0;
    }
    xio_msg_s *xio_msg = reinterpret_cast<xio_msg_s*>(i_msg.opaque());

    ovs_xio_aio_complete_request(const_cast<void*>(xio_msg->opaque),
                                 i_msg.retval(),
                                 i_msg.errval());

    reply->in.header.iov_base = NULL;
    reply->in.header.iov_len = 0;
    vmsg_sglist_set_nents(&reply->in, 0);
    xio_release_response(reply);
    delete xio_msg;
    return 0;
}

int
NetworkXioClient::on_msg_error_control(xio_session *session ATTR_UNUSED,
                                       xio_status error ATTR_UNUSED,
                                       xio_msg_direction direction,
                                       xio_msg *msg,
                                       void *cb_user_context ATTR_UNUSED)
{
    if (direction == XIO_MSG_DIRECTION_IN)
    {
        xio_release_response(msg);
    }
    else
    {
        NetworkXioMsg i_msg;
        try
        {
            i_msg.unpack_msg(static_cast<const char*>(msg->out.header.iov_base),
                             msg->out.header.iov_len);
        }
        catch (...)
        {
            //cnanakos: client logging?
            return 0;
        }
        xio_msg_s *xio_msg = reinterpret_cast<xio_msg_s*>(i_msg.opaque());
        ovs_xio_complete_request_control(const_cast<void*>(xio_msg->opaque),
                                         -1,
                                         EIO);
        delete xio_msg;
    }
    return 0;
}

int
NetworkXioClient::on_msg_control(xio_session *session ATTR_UNUSED,
                                 xio_msg *reply,
                                 int last_in_rxq ATTR_UNUSED,
                                 void *cb_user_context)
{
    xio_context *ctx = static_cast<xio_context*>(cb_user_context);
    NetworkXioMsg i_msg;
    try
    {
        i_msg.unpack_msg(static_cast<const char*>(reply->in.header.iov_base),
                         reply->in.header.iov_len);
    }
    catch (...)
    {
        //cnanakos: logging
        return 0;
    }
    xio_ctl_s *xctl = reinterpret_cast<xio_ctl_s*>(i_msg.opaque());
    ovs_xio_complete_request_control(const_cast<void*>(xctl->xmsg.opaque),
                                     i_msg.retval(),
                                     i_msg.errval());

    switch (i_msg.opcode())
    {
    case NetworkXioMsgOpcode::ListVolumesRsp:
        handle_list_volumes(xctl,
                            vmsg_sglist(&reply->in),
                            i_msg.retval());
        break;
    case NetworkXioMsgOpcode::ListSnapshotsRsp:
        handle_list_snapshots(xctl,
                              vmsg_sglist(&reply->in),
                              i_msg.retval(),
                              i_msg.size());
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
    xio_context *ctx = static_cast<xio_context*>(cb_user_context);
    switch (event_data->event)
    {
    case XIO_SESSION_CONNECTION_TEARDOWN_EVENT:
        xio_connection_destroy(event_data->conn);
        xio_context_stop_loop(ctx);
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

int
NetworkXioClient::assign_data_in_buf_control(xio_msg *msg,
                                             void *cb_user_context ATTR_UNUSED)
{
    xio_iovec_ex *sglist = vmsg_sglist(&msg->in);
    xio_reg_mem xbuf;

    if (!sglist[0].iov_len)
    {
        return 0;
    }
    xio_mem_alloc(sglist[0].iov_len, &xbuf);
    sglist[0].iov_base = xbuf.addr;
    sglist[0].mr = xbuf.mr;
    return 0;
}

xio_connection*
NetworkXioClient::create_connection_control(xio_context *ctx,
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
    s_ops.assign_data_in_buf = assign_data_in_buf_control;

    memset(&params, 0, sizeof(params));
    params.type = XIO_SESSION_CLIENT;
    params.ses_ops = &s_ops;
    params.uri = uri.c_str();
    params.user_context = ctx;

    session = xio_session_create(&params);
    if (session == NULL)
    {
        return NULL;
    }
    memset(&cparams, 0, sizeof(cparams));
    cparams.session = session;
    cparams.ctx = ctx;
    cparams.conn_user_context = ctx;

    conn = xio_connect(&cparams);
    return conn;
}

void
NetworkXioClient::xio_submit_request(const std::string& uri,
                                     xio_ctl_s *xctl,
                                     void *opaque)
{
    if (xio_init_refcnt.fetch_add(1, std::memory_order_relaxed) == 0)
    {
        xio_init();
    }

    xio_context *ctx = xio_context_create(NULL, 0, -1);
    xio_connection *conn = create_connection_control(ctx, uri);
    if (conn == NULL)
    {
        ovs_xio_complete_request_control(opaque,
                                         -1,
                                         EIO);
        return;
    }

    int ret = xio_send_request(conn, &xctl->xmsg.xreq);
    if (ret < 0)
    {
        ovs_xio_complete_request_control(opaque,
                                         -1,
                                         EIO);
        goto exit;
    }
    xio_context_run_loop(ctx, XIO_INFINITE);
exit:
    xio_disconnect(conn);
    xio_context_run_loop(ctx, 100);
    xio_context_destroy(ctx);
    if (xio_init_refcnt.fetch_sub(1, std::memory_order_relaxed) == 1)
    {
        xio_shutdown();
    }
}

void
NetworkXioClient::handle_list_volumes(xio_ctl_s *xctl,
                                      xio_iovec_ex *sglist,
                                      int vec_size)
{
    uint64_t idx = 0;
    for (int i = 0; i < vec_size; i++)
    {
       xctl->vec->push_back(static_cast<char*>(sglist[0].iov_base) + idx);
       idx += strlen(static_cast<char*>(sglist[0].iov_base)) + 1;
    }
}

void
NetworkXioClient::handle_list_snapshots(xio_ctl_s *xctl,
                                        xio_iovec_ex *sglist,
                                        int vec_size,
                                        int size)
{
    uint64_t idx = 0;
    for (int i = 0; i < vec_size; i++)
    {
       xctl->vec->push_back(static_cast<char*>(sglist[0].iov_base) + idx);
       idx += strlen(static_cast<char*>(sglist[0].iov_base)) + 1;
    }
    xctl->size = size;
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
                                    const std::string& volume_name,
                                    size_t size,
                                    void *opaque)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.opaque = opaque;
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::CreateVolumeReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);
    xctl->xmsg.msg.size(size);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), opaque);
}

void
NetworkXioClient::xio_remove_volume(const std::string& uri,
                                    const std::string& volume_name,
                                    void *opaque)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.opaque = opaque;
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::RemoveVolumeReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), opaque);
}

void
NetworkXioClient::xio_stat_volume(const std::string& uri,
                                  const std::string& volume_name,
                                  void *opaque)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->xmsg.opaque = opaque;
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::StatVolumeReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), opaque);
}

void
NetworkXioClient::xio_list_volumes(const std::string& uri,
                                   std::vector<std::string>& volumes)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->vec = &volumes;
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::ListVolumesReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), NULL);
}

void
NetworkXioClient::xio_list_snapshots(const std::string& uri,
                                     const std::string& volume_name,
                                     std::vector<std::string>& snapshots,
                                     uint64_t *size)
{
    auto xctl = std::make_unique<xio_ctl_s>();
    xctl->vec = &snapshots;
    xctl->xmsg.msg.opcode(NetworkXioMsgOpcode::ListSnapshotsReq);
    xctl->xmsg.msg.opaque((uintptr_t)xctl.get());
    xctl->xmsg.msg.volume_name(volume_name);

    xio_msg_prepare(&xctl->xmsg);
    xio_submit_request(uri, xctl.get(), NULL);
    *size = xctl->size;
}

} //namespace volumedriverfs
