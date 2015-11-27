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
#include "common.h"
#include "VolumeCacheHandler.h"
#include "ShmControlChannelClient.h"
#include "AioCompletion.h"
#include "../ShmControlChannelProtocol.h"
#include "../ShmClient.h"

#include <limits.h>
#include <map>
#include <youtils/SpinLock.h>
#include <youtils/System.h>
#include <youtils/IOException.h>

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

struct IOThread
{
    ovs_ctx_t* ctx;
    std::thread iothread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped = false;
    bool stopping = false;

    void
    reset_iothread()
    {
        stopping = true;
        {
            std::unique_lock<std::mutex> lock_(mutex_);
            cv_.wait(lock_, [&]{ return stopped == true; });
        }
        iothread_.join();
    }

    void
    stop()
    {
        stopping = true;
    }
};

typedef std::unique_ptr<IOThread> IOThreadPtr;

static void _aio_readreply_handler(void*);
static void _aio_writereply_handler(void*);

struct ovs_context_t
{
    ovs_context_t(const std::string& volume_name, int flag)
    : oflag(flag)
    {
        io_threads_pool_size_ =
        youtils::System::get_env_with_default<int>("LIBOVSVOLUMEDRIVER_IO_THREADS_POOL_SIZE",
                                                   1);
        shm_client_ = std::make_shared<volumedriverfs::ShmClient>(volume_name);
        try
        {
            ctl_client_ = std::make_shared<ShmControlChannelClient>();
        }
        catch (std::bad_alloc& e)
        {
        }

        if (not ctl_client_->connect_and_register(volume_name,
                                                  shm_client_->get_key()))
        {
            shm_client_.reset();
            ctl_client_.reset();
            throw fungi::IOException("cannot connect and register to server");
        }

        try
        {
            ovs_aio_init();
        }
        catch (std::bad_alloc&)
        {
            shm_client_.reset();
            ctl_client_.reset();
            throw;
        }
        catch (std::system_error&)
        {
            shm_client_.reset();
            ctl_client_.reset();
            throw;
        }

        try
        {
            cache_ = std::make_unique<VolumeCacheHandler>(shm_client_,
                                                          ctl_client_);
        }
        catch (std::bad_alloc&)
        {
            ovs_aio_destroy();
            shm_client_.reset();
            ctl_client_->deregister();
            ctl_client_.reset();
            throw;
        }
        int ret = cache_->preallocate();
        if (ret < 0)
        {
            cache_->drop_caches();
        }
    }

    ~ovs_context_t()
    {
        ovs_aio_destroy();
        cache_.reset();
        shm_client_.reset();
        ctl_client_->deregister();
        ctl_client_.reset();
    }

    void
    close_rr_iothreads()
    {
        for (auto& x: rr_iothreads)
        {
            x->reset_iothread();
        }
        rr_iothreads.clear();
    }
    void
    close_wr_iothreads()
    {
        for (auto& x: wr_iothreads)
        {
            x->reset_iothread();
        }
        wr_iothreads.clear();
    }

    void
    ovs_aio_init()
    {
        for (int i = 0; i < io_threads_pool_size_; i++)
        {
            IOThreadPtr iot;
            try
            {
                iot = std::make_unique<IOThread>();
                iot->ctx = this;
                iot->iothread_ = std::move(std::thread(_aio_readreply_handler,
                                                       (void*)iot.get()));
                rr_iothreads.push_back(std::move(iot));
            }
            catch (std::bad_alloc& e)
            {
                close_rr_iothreads();
                throw;
            }
            catch (std::system_error& e)
            {
                close_rr_iothreads();
                throw;
            }
        }

        for (int i = 0; i < io_threads_pool_size_; i++)
        {
            IOThreadPtr iot;
            try
            {
                iot = std::make_unique<IOThread>();
                iot->ctx = this;
                iot->iothread_ = std::move(std::thread(_aio_writereply_handler,
                                                       (void*)iot.get()));
                wr_iothreads.push_back(std::move(iot));
            }
            catch (std::bad_alloc& e)
            {
                close_wr_iothreads();
                close_rr_iothreads();
                throw;
            }
            catch (std::system_error& e)
            {
                close_wr_iothreads();
                close_rr_iothreads();
                throw;
            }
        }
    }

    void
    ovs_aio_destroy()
    {
        for (auto& iot: rr_iothreads)
        {
            iot->stop();
        }
        for (auto& iot: wr_iothreads)
        {
            iot->stop();
        }

        /* nonexcept */
        if (ctl_client_->is_connected())
        {
            shm_client_->stop_reply_queues(io_threads_pool_size_);
        }
        close_wr_iothreads();
        close_rr_iothreads();
    }

    int oflag;
    int io_threads_pool_size_;
    volumedriverfs::ShmClientPtr shm_client_;
    std::vector<IOThreadPtr> rr_iothreads;
    std::vector<IOThreadPtr> wr_iothreads;
    VolumeCacheHandlerPtr cache_;
    ShmControlChannelClientPtr ctl_client_;
};

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
_aio_wake_up_suspended_aiocb(ovs_aio_request *request)
{
    if (not __sync_bool_compare_and_swap(&request->_on_suspend,
                                         false,
                                         true,
                                         __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&request->_mutex);
        request->_signaled = true;
        pthread_cond_signal(&request->_cond);
        pthread_mutex_unlock(&request->_mutex);
    }
}

static void
_aio_request_handler(ovs_aio_request *request,
                     size_t ret,
                     bool failed)
{
    /* errno already set by shm_receive_*_reply function */
    request->_errno = errno;
    request->_rv = ret;
    request->_completed = true;
    request->_failed = failed;
    _aio_wake_up_suspended_aiocb(request);
    if (request->completion)
    {
        request->completion->_rv = ret;
        request->completion->_failed = failed;
        AioCompletion::get_aio_context().schedule(request->completion);
    }
    if (RequestOp::AsyncFlush == request->_op)
    {
        pthread_mutex_destroy(&request->_mutex);
        pthread_cond_destroy(&request->_cond);
        delete request->ovs_aiocbp;
        delete request;
    }
}

static void
_aio_readreply_handler(void *arg)
{
    IOThread *iothread = (IOThread*) arg;
    ovs_ctx_t *ctx = iothread->ctx;
    const struct timespec timeout = {2, 0};
    size_t size_in_bytes;
    bool failed;
    while (not iothread->stopping)
    {
        ovs_aio_request *request;
        failed = ctx->shm_client_->timed_receive_read_reply(size_in_bytes,
                                                            reinterpret_cast<void**>(&request),
                                                            &timeout);
        if (request)
        {
            _aio_request_handler(request,
                                 size_in_bytes,
                                 failed);
        }
    }
    std::lock_guard<std::mutex> lock_(iothread->mutex_);
    iothread->stopped = true;
    iothread->cv_.notify_all();
}

static void
_aio_writereply_handler(void *arg)
{
    IOThread *iothread = (IOThread*) arg;
    ovs_ctx_t *ctx = iothread->ctx;
    const struct timespec timeout = {2, 0};
    size_t size_in_bytes;
    bool failed;
    while (not iothread->stopping)
    {
        ovs_aio_request *request;
        failed = ctx->shm_client_->timed_receive_write_reply(size_in_bytes,
                                                             reinterpret_cast<void**>(&request),
                                                             &timeout);
        if (request)
        {
            _aio_request_handler(request,
                                 size_in_bytes,
                                 failed);
        }
    }
    std::lock_guard<std::mutex> lock_(iothread->mutex_);
    iothread->stopped = true;
    iothread->cv_.notify_all();
}

ovs_ctx_t*
ovs_ctx_init(const char* volume_name,
             int oflag)
{
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

    /* On Error: EACCESS or EIO */
    try
    {
        ovs_ctx_t *ctx = new ovs_ctx_t(volume_name, oflag);
        return ctx;
    }
    catch (ShmIdlInterface::VolumeDoesNotExist)
    {
        errno = EACCES;
        return NULL;
    }
    catch (ShmIdlInterface::VolumeNameAlreadyRegistered)
    {
        errno = EBUSY;
        return NULL;
    }
    catch (std::bad_alloc&)
    {
        errno = ENOMEM;
        return NULL;
    }
    catch (...)
    {
        errno = EIO;
        return NULL;
    }
}

int
ovs_ctx_destroy(ovs_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    delete ctx;
    return 0;
}

int
ovs_create_volume(const char *volume_name, uint64_t size)
{
    if (not _is_volume_name_valid(volume_name))
    {
        errno = EINVAL;
        return -1;
    }

    try
    {
        volumedriverfs::ShmClient::create_volume(volume_name, size);
    }
    catch (ShmIdlInterface::VolumeExists)
    {
        errno = EEXIST;
        return -1;
    }
    catch (...)
    {
        errno = EIO;
        return -1;
    }
    return 0;
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
        request = new ovs_aio_request;
        request->ovs_aiocbp = ovs_aiocbp;
        request->completion = completion;
        request->_op = op;
    }
    catch (std::bad_alloc&)
    {
        errno = ENOMEM;
        return -1;
    }

    pthread_cond_init(&request->_cond, NULL);
    pthread_mutex_init(&request->_mutex, NULL);
    request->_on_suspend = false;
    request->_canceled = false;
    request->_completed = false;
    request->_signaled = false;
    request->_rv = 0;
    ovs_aiocbp->request_ = request;
    switch (op)
    {
    case RequestOp::Read:
        /* on error returns -1 */
        ret = ctx->shm_client_->send_read_request(ovs_aiocbp->aio_buf,
                                                  ovs_aiocbp->aio_nbytes,
                                                  ovs_aiocbp->aio_offset,
                                                  reinterpret_cast<void*>(request));
        break;
    case RequestOp::Write:
    case RequestOp::Flush:
    case RequestOp::AsyncFlush:
        /* on error returns -1 */
        ret = ctx->shm_client_->send_write_request(ovs_aiocbp->aio_buf,
                                                   ovs_aiocbp->aio_nbytes,
                                                   ovs_aiocbp->aio_offset,
                                                   reinterpret_cast<void*>(request));
        break;
    default:
        ret = -1;
    }
    if (ret < 0)
    {
        delete request;
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

    if (ovs_aiocbp->request_->_canceled)
    {
        return ECANCELED;
    }

    if (not ovs_aiocbp->request_->_completed)
    {
        return EINPROGRESS;
    }

    if (ovs_aiocbp->request_->_failed)
    {
        return ovs_aiocbp->request_->_errno;
    }
    else
    {
        return 0;
    }
}

ssize_t
ovs_aio_return(ovs_ctx_t *ctx,
               struct ovs_aiocb *ovs_aiocbp)
{
    int ret;
    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    errno = ovs_aiocbp->request_->_errno;
    if (not ovs_aiocbp->request_->_failed)
    {
        ret = ovs_aiocbp->request_->_rv;
    }
    else
    {
        errno = ovs_aiocbp->request_->_errno;
        ret = -1;
    }
    return ret;
}

int
ovs_aio_finish(ovs_ctx_t *ctx,
               struct ovs_aiocb *ovs_aiocbp)
{
    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    pthread_cond_destroy(&ovs_aiocbp->request_->_cond);
    pthread_mutex_destroy(&ovs_aiocbp->request_->_mutex);
    delete ovs_aiocbp->request_;
    return 0;
}

static int
ovs_aio_suspend_on_aiocb(struct ovs_aiocb *aiocbp,
                      const struct timespec *timeout)
{
    int ret = 0;
    if (__sync_bool_compare_and_swap(&aiocbp->request_->_on_suspend,
                                     false,
                                     true,
                                     __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&aiocbp->request_->_mutex);
        while (not aiocbp->request_->_signaled)
        {
            if (timeout)
            {
                ret = pthread_cond_timedwait(&aiocbp->request_->_cond,
                                             &aiocbp->request_->_mutex,
                                             timeout);
            }
            else
            {
                ret = pthread_cond_wait(&aiocbp->request_->_cond,
                                        &aiocbp->request_->_mutex);
            }
        }
        pthread_mutex_unlock(&aiocbp->request_->_mutex);
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
    return ovs_aio_suspend_on_aiocb(ovs_aiocbp, timeout);
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
        ovs_completion_t *completion = new ovs_completion_t;
        completion->complete_cb = complete_cb;
        completion->cb_arg = arg;
        completion->_calling = false;
        completion->_on_wait = false;
        completion->_signaled = false;
        completion->_failed = false;
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
        if (not completion->_failed)
        {
            return completion->_rv;
        }
        else
        {
            errno = EIO;
            return -1;
        }
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
        while (not completion->_signaled)
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
ovs_aio_release_completion(ovs_completion_t *completion)
{
    if (completion == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    pthread_mutex_destroy(&completion->_mutex);
    pthread_cond_destroy(&completion->_cond);
    delete completion;
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

    ovs_aiocb *aio = new ovs_aiocb;
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

    ret = ovs_aio_return(ctx, &aio);
    if (ovs_aio_finish(ctx, &aio) < 0)
    {
         return -1;
    }

    return ret;
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

    ret = ovs_aio_return(ctx, &aio);
    if (ovs_aio_finish(ctx, &aio) < 0)
    {
         return -1;
    }
    return ret;
}

int
ovs_stat(ovs_ctx_t *ctx, struct stat *st)
{
    if (ctx == NULL || st == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    return ctx->shm_client_->stat(st);
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

    ret = ovs_aio_return(ctx, &aio);
    if (ovs_aio_finish(ctx, &aio) < 0)
    {
        return -1;
    }
    return ret;
}
