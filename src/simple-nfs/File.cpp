// Copyright 2015 Open vStorage NV
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

#include "File.h"
#include "Mount.h"

#include <sstream>

#include <youtils/Assert.h>
#include <fcntl.h>
extern "C"
{
#include <nfsc/libnfs.h>
}

namespace simple_nfs
{

namespace fs = boost::filesystem;
namespace yt = youtils;

namespace
{

DECLARE_LOGGER("SimpleNfsFileUtils");

struct FhDeleter
{
    explicit FhDeleter(std::shared_ptr<nfs_context>& ctx)
        : ctx_(ctx)
    {
        VERIFY(ctx_);
    }

    void
    operator()(nfsfh* fh)
    {
        if (fh)
        {
            nfs_close(ctx_.get(), fh);
        }
    }

    std::shared_ptr<nfs_context> ctx_;
};

int
translate_mode(yt::FDMode mode)
{
    switch (mode)
    {
    case yt::FDMode::Read:
        return O_RDONLY;
    case yt::FDMode::Write:
        return O_WRONLY;
    case yt::FDMode::ReadWrite:
        return O_RDWR;
    }

    UNREACHABLE;
    return -1;
}

int translate_create_mode(yt::FDMode mode)
{
    switch(mode)
    {
    case yt::FDMode::Read:
        return 0400;
    case yt::FDMode::Write:
        return 0200;
    case yt::FDMode::ReadWrite:
        return 0600;
    }

    UNREACHABLE;
    return -1;
}

}

File::File(Mount& mnt,
           const fs::path& p,
           yt::FDMode fdmode,
           CreateIfNecessary create_if_necessary)
    : ctx_(mnt.ctx_)
{

    /* mode is not the same for both nfs_creat and nfs_open. For nfs_open
     * mode has the same meaning as flags but for nfs_creat is the mode used
     * for file creation. The newer version of libnfs makes this difference
     * obvious by introducing a second field 'flags' along with mode. We should
     * consider upgrading to the latest libnfs then and drop the workaround
     * introduced below.
     */
    const int mode = create_if_necessary == CreateIfNecessary::T ?
        translate_create_mode(fdmode) : translate_mode(fdmode);

    LOG_TRACE(mnt.share_name() << ", " << p << ", mode " << mode <<
              ", create: " << create_if_necessary);

    nfsfh* fh;
    int ret;

    if (create_if_necessary == CreateIfNecessary::T)
    {
        ret = nfs_creat(ctx_.get(),
                        p.string().c_str(),
                        mode,
                        &fh);
    }
    else
    {
        ret = nfs_open(ctx_.get(),
                       p.string().c_str(),
                       mode,
                       &fh);
    }

    if (ret < 0)
    {
        const char* err = nfs_get_error(ctx_.get());
        std::stringstream ss;
        ss << "Failed to open " << p << ": " << err;
        LOG_ERROR(ss.str());
        throw Exception(ss.str().c_str());
    }
    else
    {
        fh_ = std::shared_ptr<nfsfh>(fh, FhDeleter(ctx_));
    }
}

template<typename... A>
int
File::convert_errors_(const char* desc,
                      int (*nfs_fun)(nfs_context*, nfsfh*, A...),
                      A... args)
{
    LOG_TRACE(desc);

    int ret = (*nfs_fun)(ctx_.get(),
                         fh_.get(),
                         args...);
    if (ret < 0)
    {
        const char* err = nfs_get_error(ctx_.get());
        std::stringstream ss;
        ss << desc << " failed: " << err;
        throw Exception(ss.str().c_str());
    }
    else
    {
        return ret;
    }
}

size_t
File::pread(void* buf, size_t count, off_t off)
{
    LOG_TRACE("count " << count << ", off " << off);

    int ret = convert_errors_<uint64_t,
                              uint64_t,
                              char*>("pread",
                                     &nfs_pread,
                                     off,
                                     count,
                                     static_cast<char*>(buf));

    LOG_TRACE(ret);
    return ret;
}

namespace
{

struct AsyncReadContext
{
    AsyncReadContext(File::ReadCallback cb,
                     void* ptr,
                     size_t sz)
        : fun(std::move(cb))
        , buf(ptr)
        , size(sz)
    {}

    File::ReadCallback fun;
    void* buf;
    size_t size;
};

void
read_cb(int status,
        struct nfs_context*,
        void* data,
        void* priv)
{
    LOG_TRACE(status);

    std::unique_ptr<AsyncReadContext> ctx(static_cast<AsyncReadContext*>(priv));
    if (status > 0)
    {
        VERIFY(status <= static_cast<ssize_t>(ctx->size));
        memcpy(ctx->buf, data, status);
    }

    ctx->fun(status);
}

}

void
File::pread(void* buf,
            size_t count,
            off_t off,
            ReadCallback fun)
{
    LOG_TRACE("count " << count << ", off " << off);

    auto ctx(std::make_unique<AsyncReadContext>(std::move(fun),
                                                buf,
                                                count));

    int ret = convert_errors_<uint64_t,
                              uint64_t,
                              nfs_cb,
                              void*>("pread_async",
                                     &nfs_pread_async,
                                     off,
                                     count,
                                     &read_cb,
                                     ctx.get());

    ctx.release();

    LOG_TRACE(ret);
}

size_t
File::pwrite(const void* buf, size_t count, off_t off)
{
    LOG_TRACE("count " << count << ", off " << off);

    int ret = convert_errors_<uint64_t,
                              uint64_t,
                              char*>("pwrite",
                                     &nfs_pwrite,
                                     off,
                                     count,
                                     const_cast<char*>(static_cast<const char*>(buf)));

    LOG_TRACE(ret);
    return ret;
}

namespace
{

using WriteCallbackPtr = std::unique_ptr<File::WriteCallback>;

void
write_cb(int status,
         struct nfs_context*,
         void* /* data */,
         void* priv)
{
    LOG_TRACE(status);

    std::unique_ptr<File::WriteCallback> fun(static_cast<File::WriteCallback*>(priv));
    (*fun)(status);
}

}

void
File::pwrite(const void* buf,
             size_t count,
             off_t off,
             WriteCallback wcb)
{
    LOG_TRACE("count " << count << ", off " << off);

    WriteCallbackPtr fun(new WriteCallback(std::move(wcb)));

    int ret = convert_errors_<uint64_t,
                              uint64_t,
                              char*,
                              nfs_cb,
                              void*>("pwrite_async",
                                     &nfs_pwrite_async,
                                     off,
                                     count,
                                     // grr. fix libnfs.
                                     static_cast<char*>(const_cast<void*>(buf)),
                                     &write_cb,
                                     fun.get());

    fun.release();

    LOG_TRACE(ret);
}

void
File::fsync()
{
    LOG_TRACE("");

    convert_errors_<>("fsync",
                      &nfs_fsync);
}

void
File::truncate(uint64_t len)
{
    LOG_TRACE(len);

    convert_errors_<uint64_t>("ftruncate",
                              &nfs_ftruncate,
                              len);
}

struct stat
File::stat()
{
    LOG_TRACE("");

    struct stat st;

    convert_errors_<struct stat*>("fstat",
                                  &nfs_fstat,
                                  &st);
    return st;
}

}
