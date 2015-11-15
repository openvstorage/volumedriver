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

#include "volumedriver.h"
#include "libovsvolumedriver.h"
#include "AioCompletion.h"
#include "../ShmControlChannelProtocol.h"

#include <limits.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/array.hpp>

#include <map>
#include <youtils/SpinLock.h>

#define NUM_THREADS  1

using boost::asio::local::stream_protocol;

enum class RequestOp
{
    Read,
    Write,
    Flush,
    AsyncFlush,
};

struct ovs_ctx_wrapper
{
    ovs_ctx_t *ctx;
    int n;
};

/* Only one AioCompletion instance atm */
AioCompletion* AioCompletion::_aio_completion_instance = NULL;
static int aio_completion_instances = 0;

static bool
_is_volume_name_valid(const char *volume_name)
{
    if (volume_name == NULL || strlen(volume_name) == 0 ||
            strlen(volume_name) >= NAME_MAX)
    {
        return false;
    }
    else
    {
        return true;
    }
}

static void
_aio_wake_up_suspended_aiocb(struct ovs_aiocb *aiocbp)
{
    if (not __sync_bool_compare_and_swap(&aiocbp->_on_suspend,
                                         false,
                                         true,
                                         __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&aiocbp->_mutex);
        aiocbp->_signaled = true;
        pthread_cond_signal(&aiocbp->_cond);
        pthread_mutex_unlock(&aiocbp->_mutex);
    }
}

static void
_aio_request_handler(ovs_aio_request *request,
                     ssize_t ret)
{
    /* errno already set by shm_receive_*_reply function */
    request->ovs_aiocbp->_errno = errno;
    if (request->completion)
    {
        request->completion->_rv = ret;
        AioCompletion::get_aio_context()->schedule(request->completion);
    }
    request->ovs_aiocbp->_rv = ret;
    request->ovs_aiocbp->_completed = true;
    _aio_wake_up_suspended_aiocb(request->ovs_aiocbp);
    if (RequestOp::AsyncFlush ==
            static_cast<RequestOp>(request->ovs_aiocbp->_op))
    {
        free(request->ovs_aiocbp);
    }
    delete request;
}

static void*
_aio_readreply_handler(void *arg)
{
    ovs_ctx_wrapper *wrapper = (ovs_ctx_wrapper *)arg;
    ovs_ctx_t *ctx = (ovs_ctx_t *)wrapper->ctx;
    ovs_async_threads *async_threads_ = &ctx->async_threads_;
    ovs_iothread_t *iothread = &async_threads_->rr_iothread[wrapper->n];
    ssize_t ret;
    while (not iothread->stopping)
    {
        ovs_aio_request *request;
        ret = shm_receive_read_reply(static_cast<ShmClientHandle>(ctx->shm_handle_),
                                     (void**)&request);
        if (request)
        {
            _aio_request_handler(request,
                                 ret);
        }
    }
    pthread_mutex_lock(&iothread->io_mutex);
    iothread->stopped = true;
    pthread_cond_signal(&iothread->io_cond);
    pthread_mutex_unlock(&iothread->io_mutex);
    delete wrapper;
    pthread_exit(NULL);
}

static void*
_aio_writereply_handler(void *arg)
{
    ovs_ctx_wrapper *wrapper = (ovs_ctx_wrapper *)arg;
    ovs_ctx_t *ctx = (ovs_ctx_t *)wrapper->ctx;
    ovs_async_threads *async_threads_ = &ctx->async_threads_;
    ovs_iothread_t *iothread = &async_threads_->wr_iothread[wrapper->n];
    ssize_t ret;
    while (not iothread->stopping)
    {
        ovs_aio_request *request;
        ret = shm_receive_write_reply(static_cast<ShmClientHandle>(ctx->shm_handle_),
                                      (void**)&request);
        if (request)
        {
            _aio_request_handler(request,
                                 ret);
        }
    }
    pthread_mutex_lock(&iothread->io_mutex);
    iothread->stopped = true;
    pthread_cond_signal(&iothread->io_cond);
    pthread_mutex_unlock(&iothread->io_mutex);
    delete wrapper;
    pthread_exit(NULL);
}

static int
_aio_init(ovs_ctx_t* ctx)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    ovs_async_threads *async_threads_ = &ctx->async_threads_;

    try
    {
        async_threads_->rr_iothread = new ovs_iothread_t [NUM_THREADS];
        for (int i = 0; i < NUM_THREADS; i++)
        {
            pthread_cond_init(&async_threads_->rr_iothread[i].io_cond, NULL);
            pthread_mutex_init(&async_threads_->rr_iothread[i].io_mutex, NULL);
            async_threads_->rr_iothread[i].stopped = false;
            async_threads_->rr_iothread[i].stopping = false;
        }
    }
    catch (std::bad_alloc)
    {
        pthread_attr_destroy(&attr);
        return -1;
    }

    try
    {
        async_threads_->wr_iothread = new ovs_iothread_t [NUM_THREADS];
        for (int i = 0; i < NUM_THREADS; i++)
        {
            pthread_cond_init(&async_threads_->wr_iothread[i].io_cond, NULL);
            pthread_mutex_init(&async_threads_->wr_iothread[i].io_mutex, NULL);
            async_threads_->wr_iothread[i].stopped = false;
            async_threads_->wr_iothread[i].stopping = false;
        }
    }
    catch (std::bad_alloc)
    {
        for (int i = 0; i < NUM_THREADS; i++)
        {
            pthread_cond_destroy(&async_threads_->rr_iothread[i].io_cond);
            pthread_mutex_destroy(&async_threads_->rr_iothread[i].io_mutex);
        }
        delete[] async_threads_->rr_iothread;
        pthread_attr_destroy(&attr);
        return -1;
    }

    for (int i = 0; i < NUM_THREADS; i++)
    {
        ovs_ctx_wrapper *wrapper = new ovs_ctx_wrapper();
        wrapper->ctx = ctx;
        wrapper->n = i;
        pthread_create(&async_threads_->rr_iothread[i].io_t,
                       &attr,
                       _aio_readreply_handler,
                       (void *) wrapper);
    }
    for (int i = 0; i < NUM_THREADS; i++)
    {
        ovs_ctx_wrapper *wrapper = new ovs_ctx_wrapper();
        wrapper->ctx = ctx;
        wrapper->n = i;
        pthread_create(&async_threads_->wr_iothread[i].io_t,
                       &attr,
                       _aio_writereply_handler,
                       (void *) wrapper);
    }

    aio_completion_instances++;
    pthread_attr_destroy(&attr);
    return 0;
}

static int
_aio_destroy(ovs_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return -1;
    }

    ovs_async_threads *async_threads_ = &ctx->async_threads_;

    for (int i = 0; i < NUM_THREADS; i++)
    {
        async_threads_->rr_iothread[i].stopping = true;
        async_threads_->wr_iothread[i].stopping = true;
    }

    shm_stop_reply_queues(static_cast<ShmClientHandle>(ctx->shm_handle_),
                          NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_mutex_lock(&async_threads_->rr_iothread[i].io_mutex);
        while (!async_threads_->rr_iothread[i].stopped)
        {
            pthread_cond_wait(&async_threads_->rr_iothread[i].io_cond,
                              &async_threads_->rr_iothread[i].io_mutex);
        }
        pthread_mutex_unlock(&async_threads_->rr_iothread[i].io_mutex);
    }

    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_mutex_lock(&async_threads_->wr_iothread[i].io_mutex);
        while (!async_threads_->wr_iothread[i].stopped)
        {
            pthread_cond_wait(&async_threads_->wr_iothread[i].io_cond,
                              &async_threads_->wr_iothread[i].io_mutex);
        }
        pthread_mutex_unlock(&async_threads_->wr_iothread[i].io_mutex);
    }

    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(async_threads_->rr_iothread[i].io_t, NULL);
        pthread_join(async_threads_->wr_iothread[i].io_t, NULL);
    }

    aio_completion_instances--;
    if (aio_completion_instances == 0)
    {
        AioCompletion::get_aio_context()->stop_completion_loop();
    }
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_cond_destroy(&async_threads_->rr_iothread[i].io_cond);
        pthread_mutex_destroy(&async_threads_->rr_iothread[i].io_mutex);
        pthread_cond_destroy(&async_threads_->wr_iothread[i].io_cond);
        pthread_mutex_destroy(&async_threads_->wr_iothread[i].io_mutex);
    }

    delete[] async_threads_->rr_iothread;
    delete[] async_threads_->wr_iothread;
    return 0;
}

static int
_ctl_channel_sendmsg(const ovs_ctx_t *ctx,
                     const ShmControlChannelMsg& msg)
{
    const std::string msg_str(msg.pack_msg());
    uint64_t msg_payload = msg_str.length();

    try
    {
        size_t len = boost::asio::write(*(ctx->socket_),
                                        boost::asio::buffer(&msg_payload,
                                                            header_size));
        assert(len == header_size);
        len = boost::asio::write(*(ctx->socket_),
                                 boost::asio::buffer(msg_str,
                                                     msg_payload));
        assert(len == msg_payload);
    }
    catch(boost::system::system_error&)
    {
        return -1;
    }
    return 0;
}

static int
_ctl_channel_recvmsg(const ovs_ctx_t *ctx,
                     ShmControlChannelMsg& msg)
{
    uint64_t payload_size;
    try
    {
        std::vector<char> reply;
        std::size_t len = boost::asio::read(*(ctx->socket_),
                                            boost::asio::buffer(&payload_size,
                                                                header_size));
        assert(len == header_size);
        reply.resize(payload_size);
        len = boost::asio::read(*(ctx->socket_),
                                boost::asio::buffer(reply,
                                                    payload_size));
        assert(len == payload_size);
        msg.unpack_msg(reply.data(),
                       len);
    }
    catch(boost::system::system_error&)
    {
        return -1;
    }
    return 0;
}

static int
_ctl_channel_register(ovs_ctx_t *ctx,
                      const std::string& volume_name,
                      const std::string& key)
{
    boost::system::error_code ec;
    ShmControlChannelMsg msg(ShmMsgOpcode::Register);
    msg.volume_name(volume_name);
    msg.key(key);

    ctx->io_service_.reset(new boost::asio::io_service);
    ctx->socket_.reset(new stream_protocol::socket(*(ctx->io_service_)));
    ctx->socket_->connect(stream_protocol::endpoint(ShmConnectionDetails::Endpoint()),
                          ec);
    if (ec)
    {
        goto failed;
    }

    if (_ctl_channel_sendmsg(ctx, msg) < 0)
    {
        goto failed;
    }

    if (_ctl_channel_recvmsg(ctx, msg) < 0)
    {
        goto failed;
    }

    if (msg.opcode() == ShmMsgOpcode::Success)
    {
        return 0;
    }
failed:
    ctx->io_service_.reset();
    ctx->socket_.reset();
    return -1;
}

static int
_ctl_channel_unregister(ovs_ctx_t *ctx)
{
    ShmControlChannelMsg msg(ShmMsgOpcode::Deregister);
    if (_ctl_channel_sendmsg(ctx, msg) < 0)
    {
        goto failed;
    }

    if (_ctl_channel_recvmsg(ctx, msg) < 0)
    {
        goto failed;
    }

    if (msg.opcode() == ShmMsgOpcode::Success)
    {
        return 0;
    }
failed:
    return -1;
}

static void
_ctl_channel_close(ovs_ctx_t *ctx)
{
    try
    {
        ctx->socket_->shutdown(stream_protocol::socket::shutdown_both);
        ctx->socket_->close();
    }
    catch(boost::system::system_error&)
    {
        return;
    }
}

ovs_ctx_t*
ovs_ctx_init(const char* volume_name,
             int oflag)
{
    int ret;
    if (not _is_volume_name_valid(volume_name))
    {
        errno = EINVAL;
        return NULL;
    }
    if (oflag != O_RDONLY &&
        oflag != O_WRONLY &&
        oflag != O_RDWR)
    {
        errno = EINVAL;
        return NULL;
    }

    ovs_ctx_t *ctx = (ovs_ctx_t*) calloc(1,
                                         sizeof(ovs_ctx_t));
    if (ctx == NULL)
    {
        return NULL;
    }
    /* Error: EACCES or EIO */
    ret = shm_create_handle(volume_name,
                            reinterpret_cast<ShmClientHandle*>(&ctx->shm_handle_));
    if (ret < 0)
    {
        free(ctx);
        errno = -ret;
        return NULL;
    }

    /* Error: -1 - socket is already free'd */
    ret = _ctl_channel_register(ctx,
                                volume_name,
                                shm_get_key(reinterpret_cast<ShmClientHandle>(ctx->shm_handle_)));
    if (ret < 0)
    {
        goto register_err;
    }

    ctx->oflag = oflag;
    ret = _aio_init(ctx);
    if (ret == 0)
    {
        ctx->cache_.reset(new VolumeCacheHandler(ctx->shm_handle_));
        int ret = ctx->cache_->preallocate();
        if (ret < 0)
        {
            /* drop caches to free some memory */
            ctx->cache_->drop_caches();
        }
        return ctx;
    }
    else
    {
        _ctl_channel_close(ctx);
    }
register_err:
    /*never here I hope*/
    shm_destroy_handle(static_cast<ShmClientHandle>(ctx->shm_handle_));
    free(ctx);

    errno = EIO;
    return NULL;
}

int
ovs_ctx_destroy(ovs_ctx_t *ctx)
{
    int ret;
    if (ctx == NULL)
    {
        ret = -1;
        goto err;
    }
    ret = _aio_destroy(ctx);
    if (ret < 0)
    {
        goto err;
    }
    ctx->cache_->drop_caches();
    shm_destroy_handle(static_cast<ShmClientHandle>(ctx->shm_handle_));
    ret = _ctl_channel_unregister(ctx);
    if (not ret)
    {
        _ctl_channel_close(ctx);
    }
    ctx->cache_.reset();
    ctx->socket_.reset();
    ctx->io_service_.reset();
    free(ctx);
    return ret;
err:
    errno = EINVAL;
    return ret;
}

int
ovs_create_volume(const char *volume_name, uint64_t size)
{
    int ret;
    if (not _is_volume_name_valid(volume_name))
    {
        errno = EINVAL;
        return -1;
    }
    /* Error: EEXIST, EIO */
    ret = shm_create_volume(volume_name, size);
    if (ret < 0)
    {
        errno = -ret;
        ret = -1;
    }
    return ret;
}

static int
_ovs_submit_aio_request(ovs_ctx_t *ctx,
                        struct ovs_aiocb *ovs_aiocbp,
                        ovs_completion_t *completion,
                        const RequestOp& op)
{
    int ret, accmode;
    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if ((ovs_aiocbp->aio_nbytes <= 0 ||
            ovs_aiocbp->aio_offset < 0) &&
            op != RequestOp::Flush && op != RequestOp::AsyncFlush)
    {
        errno = EINVAL;
        return -1;
    }

    accmode = ctx->oflag & O_ACCMODE;
    switch (op)
    {
    case RequestOp::Read:
        if (accmode == O_WRONLY)
        {
            errno = EBADF;
            return -1;
        }
        break;
    case RequestOp::Write:
    case RequestOp::Flush:
    case RequestOp::AsyncFlush:
        if (accmode == O_RDONLY)
        {
            errno = EBADF;
            return -1;
        }
        break;
    default:
        errno = EBADF;
        return -1;
    }

    ovs_aio_request *request;
    try
    {
        request = new ovs_aio_request();
        request->ovs_aiocbp = ovs_aiocbp;
        request->completion = completion;
    }
    catch (std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }

    pthread_cond_init(&ovs_aiocbp->_cond, NULL);
    pthread_mutex_init(&ovs_aiocbp->_mutex, NULL);
    ovs_aiocbp->_on_suspend = false;
    ovs_aiocbp->_canceled = false;
    ovs_aiocbp->_completed = false;
    ovs_aiocbp->_signaled = false;
    ovs_aiocbp->_rv = 0;
    ovs_aiocbp->_op = static_cast<int>(op);
    ovs_aiocbp->_request = request;
    switch (op)
    {
    case RequestOp::Read:
        /* on error returns -1 */
        ret = shm_send_read_request(static_cast<ShmClientHandle>(ctx->shm_handle_),
                                    ovs_aiocbp->aio_buf,
                                    ovs_aiocbp->aio_nbytes,
                                    ovs_aiocbp->aio_offset,
                                    (void*) request);
        break;
    case RequestOp::Write:
    case RequestOp::Flush:
    case RequestOp::AsyncFlush:
        /* on error returns -1 */
        ret = shm_send_write_request(static_cast<ShmClientHandle>(ctx->shm_handle_),
                                     ovs_aiocbp->aio_buf,
                                     ovs_aiocbp->aio_nbytes,
                                     ovs_aiocbp->aio_offset,
                                     (void*) request);
        break;
    default:
        ret = -1;
    }
    if (ret < 0)
    {
        errno = EIO;
    }
    return ret;
}

int
ovs_aio_read(ovs_ctx_t *ctx,
             struct ovs_aiocb *ovs_aiocbp)
{
    return _ovs_submit_aio_request(ctx,
                                   ovs_aiocbp,
                                   NULL,
                                   RequestOp::Read);
}

int
ovs_aio_write(ovs_ctx_t *ctx,
              struct ovs_aiocb *ovs_aiocbp)
{
    return _ovs_submit_aio_request(ctx,
                                   ovs_aiocbp,
                                   NULL,
                                   RequestOp::Write);
}

int
ovs_aio_error(ovs_ctx_t *ctx,
              const struct ovs_aiocb *ovs_aiocbp)
{
    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (ovs_aiocbp->_canceled)
    {
        return ECANCELED;
    }

    if (not ovs_aiocbp->_completed)
    {
        return EINPROGRESS;
    }

    if (ovs_aiocbp->_rv == -1)
    {
        errno = ovs_aiocbp->_errno;
        return -1;
    }
    else
    {
        return 0;
    }
}

ssize_t
ovs_aio_return(ovs_ctx_t *ctx,
               const struct ovs_aiocb *ovs_aiocbp)
{
    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    errno = ovs_aiocbp->_errno;
    return ovs_aiocbp->_rv;
}

static int
_aio_suspend_on_aiocb(struct ovs_aiocb *aiocbp,
                      const struct timespec *timeout)
{
    int ret = 0;
    if (__sync_bool_compare_and_swap(&aiocbp->_on_suspend,
                                     false,
                                     true,
                                     __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&aiocbp->_mutex);
        if (not aiocbp->_signaled)
        {
            if (timeout)
            {
                ret = pthread_cond_timedwait(&aiocbp->_cond,
                                             &aiocbp->_mutex,
                                             timeout);
            }
            else
            {
                ret = pthread_cond_wait(&aiocbp->_cond,
                                        &aiocbp->_mutex);
            }
        }
        pthread_mutex_unlock(&aiocbp->_mutex);
    }
    return ret;
}

int
ovs_aio_suspend(ovs_ctx_t *ctx,
                ovs_aiocb *ovs_aiocbp,
                const struct timespec *timeout)
{
    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    return _aio_suspend_on_aiocb(ovs_aiocbp, timeout);
}

int
ovs_aio_cancel(ovs_ctx_t * /*ctx*/,
               struct ovs_aiocb * /*ovs_aiocbp*/)
{
    errno = ENOSYS;
    return -1;
}

ovs_buffer_t*
ovs_allocate(ovs_ctx_t *ctx,
             size_t size)
{
    ovs_buffer_t *buf = ctx->cache_->allocate(size);
    if (!buf)
    {
        errno = ENOMEM;
    }
    return buf;
}

void*
ovs_buffer_data(ovs_buffer_t *ptr)
{
    if (likely(ptr != NULL))
    {
        return ptr->buf;
    }
    else
    {
        errno = EINVAL;
        return NULL;
    }
}

size_t
ovs_buffer_size(ovs_buffer_t *ptr)
{
    if (likely(ptr != NULL))
    {
        return ptr->size;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

int
ovs_deallocate(ovs_ctx_t *ctx,
               ovs_buffer_t *ptr)
{
    int ret = ctx->cache_->deallocate(ptr);
    if (ret < 0)
    {
        errno = EFAULT;
    }
    return ret;
}

ovs_completion_t*
ovs_aio_create_completion(ovs_callback_t complete_cb,
                          void *arg)
{
    if (complete_cb == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    try
    {
        ovs_completion_t *completion = new ovs_completion_t();
        completion->complete_cb = complete_cb;
        completion->cb_arg = arg;
        completion->_calling = false;
        completion->_on_wait = false;
        pthread_cond_init(&completion->_cond, NULL);
        pthread_mutex_init(&completion->_mutex, NULL);
        return completion;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return NULL;
    }
}

ssize_t
ovs_aio_return_completion(ovs_completion_t *completion)
{
    if (completion == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (not completion->_calling)
    {
        return -1;
    }
    else
    {
        return completion->_rv;
    }
}

int
ovs_aio_wait_completion(ovs_completion_t *completion,
                        const struct timespec *timeout)
{
    int ret = 0;
    if (completion == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (__sync_bool_compare_and_swap(&completion->_on_wait,
                                     false,
                                     true,
                                     __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&completion->_mutex);
        if (not completion->_signaled)
        {
            if (timeout)
            {
                ret = pthread_cond_timedwait(&completion->_cond,
                                             &completion->_mutex,
                                             timeout);
            }
            else
            {
                ret = pthread_cond_wait(&completion->_cond,
                                        &completion->_mutex);
            }
        }
        pthread_mutex_unlock(&completion->_mutex);
    }
    return ret;
}

int
ovs_aio_signal_completion(ovs_completion_t *completion)
{
    if (completion == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    if (not __sync_bool_compare_and_swap(&completion->_on_wait,
                                         false,
                                         true,
                                         __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&completion->_mutex);
        completion->_signaled = true;
        pthread_cond_signal(&completion->_cond);
        pthread_mutex_unlock(&completion->_mutex);
    }
    return 0;
}

int
ovs_aio_release_completion(ovs_completion_t *comp)
{
    if (comp == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    delete comp;
    return 0;
}

int
ovs_aio_readcb(ovs_ctx_t *ctx,
               struct ovs_aiocb *ovs_aiocbp,
               ovs_completion_t *completion)

{
    return _ovs_submit_aio_request(ctx,
                                   ovs_aiocbp,
                                   completion,
                                   RequestOp::Read);
}

int
ovs_aio_writecb(ovs_ctx_t *ctx,
                struct ovs_aiocb *ovs_aiocbp,
                ovs_completion_t *completion)

{
    return _ovs_submit_aio_request(ctx,
                                   ovs_aiocbp,
                                   completion,
                                   RequestOp::Write);
}

int
ovs_aio_flushcb(ovs_ctx_t *ctx,
                ovs_completion_t *completion)
{
    if (ctx == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    struct ovs_aiocb *aio = (struct ovs_aiocb*) calloc(1,
                                                       sizeof(struct ovs_aiocb));
    if (aio == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    aio->aio_nbytes = 0;
    aio->aio_offset = 0;
    aio->aio_buf = NULL;

    return _ovs_submit_aio_request(ctx,
                                   aio,
                                   completion,
                                   RequestOp::AsyncFlush);
}

ssize_t
ovs_read(ovs_ctx_t *ctx,
         void *buf,
         size_t nbytes,
         off_t offset)
{
    ssize_t ret;
    struct ovs_aiocb aio;
    aio.aio_buf = buf;
    aio.aio_nbytes = nbytes;
    aio.aio_offset = offset;

    if ((ret = ovs_aio_read(ctx, &aio)) < 0)
    {
        return ret;
    }

    if ((ret = ovs_aio_suspend(ctx, &aio, NULL)) < 0)
    {
        return ret;
    }

    return ovs_aio_return(ctx, &aio);
}

ssize_t
ovs_write(ovs_ctx_t *ctx,
          const void *buf,
          size_t nbytes,
          off_t offset)
{
    ssize_t ret;
    struct ovs_aiocb aio;
    aio.aio_buf = const_cast<void*>(buf);
    aio.aio_nbytes = nbytes;
    aio.aio_offset = offset;

    if ((ret = ovs_aio_write(ctx, &aio)) < 0)
    {
        return ret;
    }

    if ((ret = ovs_aio_suspend(ctx, &aio, NULL)) < 0)
    {
        return ret;
    }

    return ovs_aio_return(ctx, &aio);
}

int
ovs_stat(ovs_ctx_t *ctx, struct stat *st)
{
    if (ctx == NULL || st == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    return shm_stat(static_cast<ShmClientHandle>(ctx->shm_handle_),
                    st);
}

int
ovs_flush(ovs_ctx_t *ctx)
{
    int ret = 0;
    if (ctx == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    struct ovs_aiocb aio;
    aio.aio_nbytes = 0;
    aio.aio_offset = 0;
    aio.aio_buf = NULL;

    if ((ret = _ovs_submit_aio_request(ctx,
                                       &aio,
                                       NULL,
                                       RequestOp::Flush)) < 0)
    {
        return ret;
    }

    if ((ret = ovs_aio_suspend(ctx, &aio, NULL)) < 0)
    {
        return ret;
    }

    return ovs_aio_return(ctx, &aio);
}
