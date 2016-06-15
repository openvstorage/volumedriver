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

#include "Assert.h"
#include "FileDescriptor.h"

#include <fcntl.h>
#include <unistd.h>

#include <sys/file.h>
#include <sys/statvfs.h>
#include <sys/stat.h>


#include <sstream>

namespace youtils
{

FileDescriptorException::FileDescriptorException(int errnum,
                                                 Exception e)
{
    std::stringstream ss;

    switch (e)
    {
    case Exception::OpenException:
        ss << "open";
        break;
    case Exception::SeekException:
        ss << "seek";
        break;
    case Exception::ReadException:
        ss << "read";
        break;
    case Exception::CloseException:
        ss << "close";
        break;
    case Exception::SyncException:
        ss << "sync";
        break;
    case Exception::WriteException:
        ss << "write";
        break;
    case Exception::StatException:
        ss << "stat";
        break;
    case Exception::FallocateException:
        ss << "fallocate";
        break;
    case Exception::TruncateException:
        ss << "truncate";
        break;
    case Exception::LockException:
        ss << "lock";
        break;
    case Exception::UnlockException:
        ss << "unlock";
        break;
    default:
        UNREACHABLE
    }

    ss << " failed: " << strerror(errnum);
    what_ = ss.str();
}

const char*
FileDescriptorException::what() const throw ()
{
    return what_.c_str();
}


FileDescriptor::FileDescriptor(const fs::path& path,
                               FDMode mode,
                               CreateIfNecessary create_if_necessary,
                               SyncOnCloseAndDestructor sync_on_close_and_destructor)
    : path_(path)
    , mode_(mode)
    , sync_on_close_and_destructor_(sync_on_close_and_destructor)
{
    int flags = T(create_if_necessary) ? O_CREAT : 0;

    switch (mode_)
    {
    case FDMode::Read:
        flags |= O_RDONLY;
        break;
    case FDMode::Write:
        flags |= O_WRONLY;
        break;
    case FDMode::ReadWrite:
        flags |= O_RDWR;
        break;
    }

    fd_ = ::open(path.string().c_str(),
                 flags,
                 S_IWUSR|S_IRUSR);

    if (fd_ < 0)
     {
         throw FileDescriptorException(errno,
                                       FileDescriptorException::Exception::OpenException);
     }
}

FileDescriptor::~FileDescriptor()
{
     if (T(sync_on_close_and_destructor_))
    {
        try
        {
            sync();
        }
        catch (std::exception& e)
        {
            LOG_ERROR(path_ << ": failed to sync: " << e.what());
        }
        catch (...)
        {
            LOG_ERROR(path_ << ": failed to sync: unknown exception");
        }
    }
    retry:
    int ret = ::close(fd_);
    if(ret == -1)
    {
        if(errno == EINTR)
        {
            goto retry;
        }
        else
        {
            LOG_ERROR("Failed to close file descriptor " << fd_ << ": " <<
                      strerror(errno));
        }
    }
}

const fs::path&
FileDescriptor::path() const
{
    return path_;
}


size_t
FileDescriptor::read(void* const buf,
                     size_t size)
{
    ssize_t s = ::read(fd_, buf, size);
    if (s < 0)
    {
        throw FileDescriptorException(errno,
                                    FileDescriptorException::Exception::ReadException);
    }

    return s;
}

size_t
FileDescriptor::pread(void* const buf,
                      size_t size,
                      off_t pos)
{
    ssize_t s = ::pread(fd_, buf, size, pos);
    if (s < 0)
    {
        throw FileDescriptorException(errno,
                                    FileDescriptorException::Exception::ReadException);
    }
    return s;
}

size_t
FileDescriptor::write(const void* const buf,
                      size_t size)
{
    ssize_t s = ::write(fd_, buf, size);
    if (s != (ssize_t)size)
    {
        throw FileDescriptorException(errno,
                                    FileDescriptorException::Exception::WriteException);
    }
    return s;
}

size_t
FileDescriptor::pwrite(const void* const buf,
                       size_t size,
                       off_t pos)
{
    ssize_t s = ::pwrite(fd_, buf, size, pos);
    if (s != (ssize_t)size)
    {
        throw FileDescriptorException(errno,
                                    FileDescriptorException::Exception::WriteException);
    }
    return s;
}

off_t
FileDescriptor::seek(off_t offset,
                     Whence w)
{
    off_t off = ::lseek(fd_, offset, get_whence_(w));
    if (off == - 1)
    {
        throw FileDescriptorException(errno,
                                    FileDescriptorException::Exception::SeekException);
    }
    return off;
}

off_t
FileDescriptor::tell()
{
    return seek(0, Whence::SeekCur);
}

void
FileDescriptor::sync()
{
    if (mode_ == FDMode::Write or
        mode_ == FDMode::ReadWrite)
    {
        int ret = ::fdatasync(fd_);
        if (ret < 0)
        {
            throw FileDescriptorException(errno,
                                        FileDescriptorException::Exception::SyncException);

        }
    }
}

int
FileDescriptor::get_whence_(Whence w)
{
    switch (w)
    {
        case Whence::SeekSet:
            return SEEK_SET;
        case Whence::SeekCur:
            return SEEK_CUR;
        case Whence::SeekEnd:
            return SEEK_END;
        default:
            VERIFY(0 == "you should not get here");
    }
}

void
FileDescriptor::statvfs_(struct statvfs& st)
{
    int ret = fstatvfs(fd_, &st);
    if (ret < 0)
    {
        throw FileDescriptorException(errno,
                                    FileDescriptorException::Exception::StatException);
    }
}

uint64_t
FileDescriptor::filesystemSize()
{
    struct statvfs st;
    statvfs_(st);
    return st.f_frsize * st.f_blocks;
}

uint64_t
FileDescriptor::filesystemFreeSize()
{
    struct statvfs st;
    statvfs_(st);
    // this is the free space available for non-root users, f_bfree contains
    // the free space for root. better not go there.
    return st.f_bavail * st.f_frsize;
}

void
FileDescriptor::fallocate(uint64_t size)
{
    int ret = posix_fallocate(fd_, 0, size);
    if (ret != 0)
    {
        throw FileDescriptorException(ret,
                                    FileDescriptorException::Exception::FallocateException);
    }
}

void
FileDescriptor::stat_(struct stat& st)
{
    int ret = ::fstat(fd_, &st);
    if (ret < 0)
    {
        throw FileDescriptorException(errno,
                                    FileDescriptorException::Exception::StatException);
    }
}

uint64_t
FileDescriptor::size()
{
    struct stat st;
    stat_(st);
    return st.st_size;
}

void
FileDescriptor::lock()
{
    ASSERT(fd_ >= 0);

    int ret = ::flock(fd_,
                      LOCK_EX);
    if (ret < 0)
    {
        throw FileDescriptorException(errno,
                                    FileDescriptorException::Exception::LockException);
    }
}

bool
FileDescriptor::try_lock()
{
    ASSERT(fd_ >= 0);

    int ret = ::flock(fd_,
                      LOCK_EX bitor LOCK_NB);
    if (ret < 0)
    {
        switch (errno)
        {
        case EWOULDBLOCK:
            return false;
        default:
            throw FileDescriptorException(errno,
                                        FileDescriptorException::Exception::LockException);
        }
    }

    return true;
}

void
FileDescriptor::unlock()
{
    ASSERT(fd_ >= 0);

    int ret = ::flock(fd_,
                      LOCK_UN);
    if (ret < 0)
    {
        throw FileDescriptorException(errno,
                                    FileDescriptorException::Exception::UnlockException);
    }
}

void
FileDescriptor::truncate(uint64_t length)
{
    ASSERT(fd_ >= 0);

    int ret = ::ftruncate(fd_, length);
    if (ret < 0)
    {
        throw FileDescriptorException(errno,
                                    FileDescriptorException::Exception::TruncateException);
    }
}

void
FileDescriptor::fadvise(FAdvise adv)
{
    int fadv = POSIX_FADV_NORMAL;

    switch (adv)
    {
    case FAdvise::DontNeed:
        fadv = POSIX_FADV_DONTNEED;
        break;
    }

    ASSERT(fd_ >= 0);
    int ret = ::posix_fadvise(fd_,
                              0,
                              0,
                              fadv);
    if (ret < 0)
    {
        throw FileDescriptorException(errno,
                                      FileDescriptorException::Exception::FAdviseException);
    }
}

}
