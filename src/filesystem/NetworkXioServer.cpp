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

#include "NetworkXioServer.h"
#include "NetworkXioProtocol.h"

#include <libxio.h>

#include <youtils/Assert.h>
#include <youtils/System.h>

namespace yt = youtils;

namespace volumedriverfs
{

namespace yt = youtils;

template<class T>
static int
static_on_request(xio_session *session,
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
    return obj->server->on_request(session,
                                   req,
                                   last_in_rxq,
                                   cb_user_context);
}

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
    return obj->on_session_event(session,
                                 event_data);
}

template<class T>
static int
static_on_new_session(xio_session *session,
                      xio_new_session_req *req,
                      void *cb_user_context)
{
    T *obj = reinterpret_cast<T*>(cb_user_context);
    if (obj == NULL)
    {
        assert(obj != NULL);
        return -1;
    }
    return obj->on_new_session(session,
                               req);
}

template<class T>
static int
static_on_msg_send_complete(xio_session *session,
                            xio_msg *msg,
                            void *cb_user_context)
{
    T *obj = reinterpret_cast<T*>(cb_user_context);
    if (obj == NULL)
    {
        assert(obj != NULL);
        return -1;
    }
    return obj->server->on_msg_send_complete(session,
                                             msg,
                                             cb_user_context);
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

NetworkXioServer::NetworkXioServer(FileSystem& fs,
                                   const yt::Uri& uri,
                                   size_t snd_rcv_queue_depth,
                                   unsigned int workqueue_max_threads)
    : fs_(fs)
    , uri_(uri)
    , stopping(false)
    , stopped(true)
    , evfd()
    , queue_depth(snd_rcv_queue_depth)
    , wq_max_threads(workqueue_max_threads)
{}

void
NetworkXioServer::xio_destroy_ctx_shutdown(xio_context *ctx)
{
    xio_context_destroy(ctx);
    xio_shutdown();
}

NetworkXioServer::~NetworkXioServer()
{
    shutdown();
}

void
NetworkXioServer::evfd_stop_loop(int /*fd*/, int /*events*/, void * /*data*/)
{
    evfd.readfd();
    xio_context_stop_loop(ctx.get());
}

void
NetworkXioServer::run(std::promise<void> promise)
{
    int xopt = 2;

    xio_init();

    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_MAX_IN_IOVLEN,
                &xopt, sizeof(int));

    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_MAX_OUT_IOVLEN,
                &xopt, sizeof(int));

    xopt = 0;
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

    struct xio_options_keepalive ka;
    ka.time =
        yt::System::get_env_with_default<int>("NETWORK_XIO_KEEPALIVE_TIME",
                                              600);
    ka.intvl =
        yt::System::get_env_with_default<int>("NETWORK_XIO_KEEPALIVE_INTVL",
                                              60);
    ka.probes =
        yt::System::get_env_with_default<int>("NETWORK_XIO_KEEPALIVE_PROBES",
                                              20);

    xio_set_opt(NULL,
                XIO_OPTLEVEL_ACCELIO,
                XIO_OPTNAME_CONFIG_KEEPALIVE,
                &ka,
                sizeof(ka));

    int polling_timeout_us =
    yt::System::get_env_with_default<int>("NETWORK_XIO_POLLING_TIMEOUT_US",
                                          0);

    ctx = std::shared_ptr<xio_context>(xio_context_create(NULL,
                                                          polling_timeout_us,
                                                          -1),
                                       xio_destroy_ctx_shutdown);

    if (ctx == nullptr)
    {
        LOG_FATAL("failed to create XIO context");
        throw FailedCreateXioContext("failed to create XIO context");
    }

    xio_session_ops xio_s_ops;
    xio_s_ops.on_session_event = static_on_session_event<NetworkXioServer>;
    xio_s_ops.on_new_session = static_on_new_session<NetworkXioServer>;
    xio_s_ops.on_msg_send_complete = static_on_msg_send_complete<NetworkXioClientData>;
    xio_s_ops.on_msg = static_on_request<NetworkXioClientData>;
    xio_s_ops.assign_data_in_buf = NULL;
    xio_s_ops.on_msg_error = static_on_msg_error<NetworkXioServer>;

    LOG_INFO("bind XIO server to '" << uri_ << "'");
    server = std::shared_ptr<xio_server>(xio_bind(ctx.get(),
                                                  &xio_s_ops,
                                                  boost::lexical_cast<std::string>(uri_).c_str(),
                                                  NULL,
                                                  0,
                                                  this),
                                         xio_unbind);
    if (server == nullptr)
    {
        LOG_FATAL("failed to bind XIO server to '" << uri_ << "'");
        throw FailedBindXioServer("failed to bind XIO server");
    }

    if(xio_context_add_ev_handler(ctx.get(),
                                  evfd,
                                  XIO_POLLIN,
                                  static_evfd_stop_loop<NetworkXioServer>,
                                  this))
    {
        LOG_FATAL("failed to register event handler");
        throw FailedRegisterEventHandler("failed to register event handler");
    }

    try
    {
        wq_ = std::make_shared<NetworkXioWorkQueue>("ovs_xio_wq",
                                                    evfd,
                                                    wq_max_threads);
    }
    catch (const WorkQueueThreadsException&)
    {
        LOG_FATAL("failed to create workqueue thread pool");
        xio_context_del_ev_handler(ctx.get(), evfd);
        throw;
    }
    catch (const std::bad_alloc&)
    {
        LOG_FATAL("failed to allocate requested storage space for workqueue");
        xio_context_del_ev_handler(ctx.get(), evfd);
        throw;
    }

    xio_mpool = std::shared_ptr<xio_mempool>(
            xio_mempool_create(-1, XIO_MEMPOOL_FLAG_REG_MR),
            xio_mempool_destroy);
    if (xio_mpool == nullptr)
    {
        LOG_FATAL("failed to create XIO memory pool");
        xio_context_del_ev_handler(ctx.get(), evfd);
        throw FailedCreateXioMempool("failed to create XIO memory pool");
    }
    (void) xio_mempool_add_slab(xio_mpool.get(),
                                4096,
                                0,
                                queue_depth,
                                32,
                                0);
    (void) xio_mempool_add_slab(xio_mpool.get(),
                                32768,
                                0,
                                queue_depth,
                                32,
                                0);
    (void) xio_mempool_add_slab(xio_mpool.get(),
                                65536,
                                0,
                                queue_depth,
                                32,
                                0);
    (void) xio_mempool_add_slab(xio_mpool.get(),
                                131072,
                                0,
                                256,
                                32,
                                0);
    (void) xio_mempool_add_slab(xio_mpool.get(),
                                1048576,
                                0,
                                32,
                                4,
                                0);
    stopped = false;
    promise.set_value();
    while (not stopping)
    {
        int ret = xio_context_run_loop(ctx.get(), XIO_INFINITE);
        VERIFY(ret == 0);
        while (not wq_->is_finished_empty())
        {
            xio_send_reply(wq_->get_finished());
        }
    }
    server.reset();
    wq_->shutdown();
    xio_context_del_ev_handler(ctx.get(), evfd);
    ctx.reset();
    xio_mpool.reset();
    std::lock_guard<std::mutex> lock_(mutex_);
    stopped = true;
    cv_.notify_one();
}

NetworkXioClientData*
NetworkXioServer::allocate_client_data()
{
    try
    {
        NetworkXioClientData *cd = new NetworkXioClientData;
        cd->disconnected = false;
        cd->refcnt = 0;
        cd->mpool = xio_mpool.get();
        cd->server = this;
        return cd;
    }
    catch (const std::bad_alloc&)
    {
        return NULL;
    }
}

int
NetworkXioServer::create_session_connection(xio_session *session,
                                            xio_session_event_data *evdata)
{
    NetworkXioClientData *cd = allocate_client_data();
    if (cd)
    {
        try
        {
            NetworkXioIOHandler *ioh_ptr = new NetworkXioIOHandler(fs_,
                                                                   wq_,
                                                                   cd);
            cd->ioh = ioh_ptr;
            cd->session = session;
            cd->conn = evdata->conn;
        }
        catch (...)
        {
            LOG_ERROR("cannot create IO handler");
            delete cd;
            return -1;
        }
        xio_connection_attr xconattr;
        xconattr.user_context = cd;
        (void) xio_modify_connection(evdata->conn,
                                     &xconattr,
                                     XIO_CONNECTION_ATTR_USER_CTX);
        return 0;
    }
    LOG_ERROR("cannot allocate client data");
    return -1;
}

void
NetworkXioServer::destroy_session_connection(xio_session *session ATTR_UNUSED,
                                             xio_session_event_data *evdata)
{
    auto cd = static_cast<NetworkXioClientData*>(evdata->conn_user_context);
    cd->disconnected = true;
    cd->ioh->remove_fs_client_info();
    if (!cd->refcnt)
    {
        xio_connection_destroy(cd->conn);
        delete cd->ioh;
        delete cd;
    }
}

int
NetworkXioServer::on_new_session(xio_session *session,
                                 xio_new_session_req * /*req*/)
{
    if (xio_accept(session, NULL, 0, NULL, 0) < 0)
    {
        LOG_ERROR("cannot accept new session, error: "
                  << xio_strerror(xio_errno()));
    }
    return 0;
}

int
NetworkXioServer::on_session_event(xio_session *session,
                                   xio_session_event_data *event_data)
{
    switch (event_data->event)
    {
    case XIO_SESSION_NEW_CONNECTION_EVENT:
        create_session_connection(session, event_data);
        break;
    case XIO_SESSION_CONNECTION_CLOSED_EVENT:
        if (event_data->reason == XIO_E_TIMEOUT)
        {
            xio_disconnect(event_data->conn);
        }
        break;
    case XIO_SESSION_CONNECTION_ERROR_EVENT:
        break;
    case XIO_SESSION_CONNECTION_DISCONNECTED_EVENT:
        break;
    case XIO_SESSION_CONNECTION_TEARDOWN_EVENT:
        destroy_session_connection(session, event_data);
        break;
    case XIO_SESSION_TEARDOWN_EVENT:
        xio_session_destroy(session);
        break;
    default:
        break;
    };
    return 0;
}

NetworkXioRequest*
NetworkXioServer::allocate_request(NetworkXioClientData *cd,
                                   xio_msg *xio_req)
{
    try
    {
        NetworkXioRequest *req = new NetworkXioRequest;
        req->xio_req = xio_req;
        req->cd = cd;
        req->data = NULL;
        req->data_len = 0;
        req->retval = 0;
        req->errval = 0;
        req->from_pool = true;
        cd->refcnt++;
        return req;
    }
    catch (const std::bad_alloc&)
    {
        return NULL;
    }
}

void
NetworkXioServer::deallocate_request(NetworkXioRequest *req)
{
    if ((req->op == NetworkXioMsgOpcode::ReadRsp ||
         req->op == NetworkXioMsgOpcode::ListVolumesRsp ||
         req->op == NetworkXioMsgOpcode::ListSnapshotsRsp ||
         req->op == NetworkXioMsgOpcode::ListClusterNodeURIRsp) && req->data)
    {
        if (req->from_pool)
        {
            xio_mempool_free(&req->reg_mem);
        }
        else
        {
            xio_mem_free(&req->reg_mem);
        }
    }
    free_request(req);
}

void
NetworkXioServer::free_request(NetworkXioRequest *req)
{
   NetworkXioClientData *cd = req->cd;
   cd->refcnt--;
   if (cd->disconnected && !cd->refcnt)
   {
       xio_connection_destroy(cd->conn);
       delete cd->ioh;
       delete cd;
   }
   delete req;
}

int
NetworkXioServer::on_msg_send_complete(xio_session *session ATTR_UNUSED,
                                       xio_msg *msg,
                                       void *cb_user_ctx ATTR_UNUSED)
{
    NetworkXioRequest *req = reinterpret_cast<NetworkXioRequest*>(msg->user_context);
    deallocate_request(req);
    return 0;
}

int
NetworkXioServer::on_msg_error(xio_session *session,
                               xio_status error,
                               xio_msg_direction direction,
                               xio_msg *msg)
{
    if (direction == XIO_MSG_DIRECTION_OUT)
    {
        NetworkXioRequest *req = reinterpret_cast<NetworkXioRequest*>(msg->user_context);
        deallocate_request(req);
    }
    else
    {
        LOG_ERROR("[" << session << "] message " << msg->request->sn
                  << " failed. reason: " << xio_strerror(error));
        xio_release_response(msg);
    }
    return 0;
}

void
NetworkXioServer::xio_send_reply(NetworkXioRequest *req)
{
    xio_msg *xio_req = req->xio_req;

    memset(&req->xio_reply, 0, sizeof(xio_msg));

    vmsg_sglist_set_nents(&req->xio_req->in, 0);
    xio_req->in.header.iov_base = NULL;
    xio_req->in.header.iov_len = 0;
    req->xio_reply.request = xio_req;

    req->xio_reply.out.header.iov_base =
        const_cast<void*>(reinterpret_cast<const void*>(req->s_msg.c_str()));
    req->xio_reply.out.header.iov_len = req->s_msg.length();
    if ((req->op == NetworkXioMsgOpcode::ReadRsp ||
         req->op == NetworkXioMsgOpcode::ListVolumesRsp ||
         req->op == NetworkXioMsgOpcode::ListSnapshotsRsp ||
         req->op == NetworkXioMsgOpcode::ListClusterNodeURIRsp ||
         req->op == NetworkXioMsgOpcode::GetVolumeURIRsp) && req->data)
    {
        vmsg_sglist_set_nents(&req->xio_reply.out, 1);
        req->xio_reply.out.sgl_type = XIO_SGL_TYPE_IOV;
        req->xio_reply.out.data_iov.max_nents = XIO_IOVLEN;
        req->xio_reply.out.data_iov.sglist[0].iov_base = req->data;
        req->xio_reply.out.data_iov.sglist[0].iov_len = req->data_len;
        req->xio_reply.out.data_iov.sglist[0].mr = req->reg_mem.mr;
    }
    req->xio_reply.flags = XIO_MSG_FLAG_IMM_SEND_COMP;
    req->xio_reply.user_context = reinterpret_cast<void*>(req);

    int ret = xio_send_response(&req->xio_reply);
    if (ret != 0)
    {
        LOG_ERROR("failed to send reply: " << xio_strerror(xio_errno()));
        deallocate_request(req);
    }
}

int
NetworkXioServer::on_request(xio_session *session ATTR_UNUSED,
                             xio_msg *xio_req,
                             int last_in_rxq ATTR_UNUSED,
                             void *cb_user_ctx)
{
    auto cd = static_cast<NetworkXioClientData*>(cb_user_ctx);
    NetworkXioRequest *req = allocate_request(cd, xio_req);
    if (req)
    {
        cd->ioh->handle_request(req);
    }
    else
    {
        int ret = xio_cancel(xio_req, XIO_E_MSG_CANCELED);
        LOG_ERROR("failed to allocate request, cancelling XIO request: "
                  << ret);
    }
    return 0;
}

void
NetworkXioServer::shutdown()
{
    if (not stopped)
    {
        stopping = true;
        xio_context_stop_loop(ctx.get());
        {
            std::unique_lock<std::mutex> lock_(mutex_);
            cv_.wait(lock_, [&]{return stopped == true;});
        }
    }
}

} //namespace
