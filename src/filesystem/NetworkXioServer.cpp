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

#include <libxio.h>

#include <youtils/Assert.h>

#include "NetworkXioServer.h"
#include "NetworkXioProtocol.h"

#define POLLING_TIME_USEC   20

namespace volumedriverfs
{

MAKE_EXCEPTION(FailedBindXioServer, fungi::IOException);
MAKE_EXCEPTION(FailedCreateXioContext, fungi::IOException);
MAKE_EXCEPTION(FailedCreateXioMempool, fungi::IOException);

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
    return obj->server->assign_data_in_buf(msg);
}

NetworkXioServer::NetworkXioServer(FileSystem& fs,
                                   const std::string& uri)
    : fs_(fs)
    , uri_(uri)
    , stopping(false)
    , stopped(false)
{
    int xopt = 2;
    int queue_depth = 2048;

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

    ctx = xio_context_create(NULL, POLLING_TIME_USEC, -1);

    if (ctx == NULL)
    {
        LOG_FATAL("failed to create XIO context");
        throw FailedCreateXioContext("failed to create XIO context");
    }

    xio_session_ops xio_s_ops;
    xio_s_ops.on_session_event = static_on_session_event<NetworkXioServer>;
    xio_s_ops.on_new_session = static_on_new_session<NetworkXioServer>;
    xio_s_ops.on_msg_send_complete = static_on_msg_send_complete<NetworkXioClientData>;
    xio_s_ops.on_msg = static_on_request<NetworkXioClientData>;
    xio_s_ops.assign_data_in_buf = static_assign_data_in_buf<NetworkXioClientData>;
    xio_s_ops.on_msg_error = NULL;

    LOG_INFO("bind XIO server to '" << uri << "'");
    server = xio_bind(ctx, &xio_s_ops, uri.c_str(), NULL, 0, this);
    if (server == NULL)
    {
        LOG_FATAL("failed to bind XIO server to '" << uri << "'");
        throw FailedBindXioServer("failed to bind XIO server");
    }

    try
    {
        wq_ = std::make_shared<NetworkXioWorkQueue>("ovs_xio_wq", ctx);
    }
    catch (const WorkQueueThreadsException&)
    {
        LOG_FATAL("failed to create workqueue thread pool");
        xio_unbind(server);
        xio_context_destroy(ctx);
        throw;
    }
    catch (const std::bad_alloc&)
    {
        LOG_FATAL("failed to allocate requested storage space for workqueue");
        xio_unbind(server);
        xio_context_destroy(ctx);
        throw;
    }

    xio_mpool = xio_mempool_create(-1, XIO_MEMPOOL_FLAG_REG_MR);
    if (!xio_mpool)
    {
        xio_unbind(server);
        xio_context_destroy(ctx);
        LOG_FATAL("failed to create XIO memory pool");
        throw FailedCreateXioMempool("failed to create XIO memory pool");
    }

    int ret = xio_mempool_add_slab(xio_mpool,
                               4096,
                               0,
                               queue_depth,
                               32,
                               0);
    if (ret < 0)
    {
        LOG_ERROR("cannot allocate 4KB slab");
    }
    ret = xio_mempool_add_slab(xio_mpool,
                               32768,
                               0,
                               queue_depth,
                               32,
                               0);
    if (ret < 0)
    {
        LOG_ERROR("cannot allocate 32KB slab");
    }
    ret = xio_mempool_add_slab(xio_mpool,
                               65536,
                               0,
                               queue_depth,
                               32,
                               0);
    if (ret < 0)
    {
        LOG_ERROR("cannot allocate 64KB slab");
    }
    ret = xio_mempool_add_slab(xio_mpool,
                               131072,
                               0,
                               256,
                               32,
                               0);
    if (ret < 0)
    {
        LOG_ERROR("cannot allocate 128KB slab");
    }
    ret = xio_mempool_add_slab(xio_mpool,
                               1048576,
                               0,
                               32,
                               4,
                               0);
    if (ret < 0)
    {
        LOG_ERROR("cannot allocate 1MB slab");
    }
}

NetworkXioServer::~NetworkXioServer()
{
    shutdown();
}

void
NetworkXioServer::run()
{
    while (not stopping)
    {
        int ret = xio_context_run_loop(ctx, XIO_INFINITE);
        VERIFY(ret == 0);
        while (not wq_->is_finished_empty())
        {
            xio_send_reply(wq_->get_finished());
        }
    }
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
        cd->mpool = xio_mpool;
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
                                                                   wq_);
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
        wq_->open_sessions_inc();
        return 0;
    }
    LOG_ERROR("cannot allocate client data");
    return -1;
}

void
NetworkXioServer::destroy_session_connection(xio_session *session __attribute__((unused)),
                                             xio_session_event_data *evdata)
{
    auto cd = static_cast<NetworkXioClientData*>(evdata->conn_user_context);
    cd->disconnected = true;
    if (!cd->refcnt)
    {
        xio_connection_destroy(cd->conn);
        wq_->open_sessions_dec();
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

int
NetworkXioServer::assign_data_in_buf(struct xio_msg *msg)
{
    xio_iovec_ex *isglist = vmsg_sglist(&msg->in);

    LOG_DEBUG("assign buffer, len: " << isglist[0].iov_len);

    xio_reg_mem in_xbuf;
    xio_mem_alloc(isglist[0].iov_len, &in_xbuf);
    isglist[0].iov_base = in_xbuf.addr;
    isglist[0].mr = in_xbuf.mr;
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
        req->work.obj = this;
        req->data = NULL;
        req->data_len = 0;
        req->retval = 0;
        req->errval = 0;
        req->from_pool = true;
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
    if (req->op == NetworkXioMsgOpcode::ReadRsp && req->data)
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
       wq_->open_sessions_dec();
       delete cd->ioh;
       delete cd;
   }
   delete req;
}

int
NetworkXioServer::on_msg_send_complete(xio_session *session __attribute__((unused)),
                                       xio_msg *msg __attribute__((unused)),
                                       void *cb_user_ctx)
{
    NetworkXioClientData *cd = static_cast<NetworkXioClientData*>(cb_user_ctx);
    NetworkXioRequest *req = cd->done_reqs.front();
    cd->done_reqs.pop_front();
    deallocate_request(req);
    return 0;
}

void
NetworkXioServer::xio_send_reply(Work *work)
{
    NetworkXioRequest *req = get_container_of(work, NetworkXioRequest, work);
    xio_msg *xio_req = req->xio_req;

    memset(&req->xio_reply, 0, sizeof(xio_msg));

    vmsg_sglist_set_nents(&req->xio_req->in, 0);
    xio_req->in.header.iov_base = NULL;
    xio_req->in.header.iov_len = 0;
    req->xio_reply.request = xio_req;

    req->xio_reply.out.header.iov_base =
        const_cast<void*>(reinterpret_cast<const void*>(req->s_msg.c_str()));
    req->xio_reply.out.header.iov_len = req->s_msg.length();
    if (req->op == NetworkXioMsgOpcode::ReadRsp && req->data)
    {
        vmsg_sglist_set_nents(&req->xio_reply.out, 1);
        req->xio_reply.out.sgl_type = XIO_SGL_TYPE_IOV;
        req->xio_reply.out.data_iov.max_nents = XIO_IOVLEN;
        req->xio_reply.out.data_iov.sglist[0].iov_base = req->data;
        req->xio_reply.out.data_iov.sglist[0].iov_len = req->data_len;
        req->xio_reply.out.data_iov.sglist[0].mr = req->reg_mem.mr;
    }
    req->xio_reply.flags = XIO_MSG_FLAG_IMM_SEND_COMP;

    int ret = xio_send_response(&req->xio_reply);
    if (ret != 0)
    {
        LOG_ERROR("failed to send reply: " << xio_strerror(xio_errno()));
        deallocate_request(req);
    }
    else
    {
        req->cd->done_reqs.push_back(req);
    }
}

int
NetworkXioServer::on_request(xio_session *session __attribute__((unused)),
                             xio_msg *xio_req,
                             int last_in_rxq __attribute__((unused)),
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
        xio_unbind(server);
        wq_->shutdown();
        stopping = true;
        xio_context_stop_loop(ctx);
        {
            std::unique_lock<std::mutex> lock_(mutex_);
            cv_.wait(lock_, [&]{return stopped == true;});
        }
        xio_mempool_destroy(xio_mpool);
        xio_context_destroy(ctx);
        xio_shutdown();
    }
}

} //namespace
