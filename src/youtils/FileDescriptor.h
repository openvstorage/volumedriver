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

#ifndef FILEDESCRIPTOR_H_
#define FILEDESCRIPTOR_H_

#include <boost/filesystem.hpp>
#include "BooleanEnum.h"
#include "Logging.h"

BOOLEAN_ENUM(CreateIfNecessary);
BOOLEAN_ENUM(SyncOnCloseAndDestructor);
struct statvfs;
struct stat;

namespace youtilstest
{
class FileDescriptorTest;
}

namespace youtils
{

namespace fs = boost::filesystem;
class FileDescriptorException
    : public std::exception
{
public:
    enum class Exception
    {
        OpenException,
        SeekException,
        ReadException,
        WriteException,
        SyncException,
        CloseException,
        StatException,
        FallocateException,
        TruncateException,
        LockException,
        UnlockException,
        FAdviseException,
    };

    FileDescriptorException(int errnum,
                          Exception e);

    virtual ~FileDescriptorException() throw ()
    {}

    virtual const char*
    what() const throw ();

private:
    std::string what_;
};


enum class FDMode
{
    Read,
    Write,
    ReadWrite
};

enum class Whence
{
    SeekSet,
    SeekCur,
    SeekEnd
};

enum class FAdvise
{
    DontNeed,
};

class FileDescriptor
{
    friend class youtilstest::FileDescriptorTest;

public:
    FileDescriptor(const fs::path& path,
                   FDMode mode,
                   CreateIfNecessary create_if_necessary = CreateIfNecessary::F,
                   SyncOnCloseAndDestructor sync_on_close_and_destructor = SyncOnCloseAndDestructor::T);

    FileDescriptor() = delete;

    FileDescriptor(const FileDescriptor& other) = delete;

    FileDescriptor(FileDescriptor&& other) = delete;

    FileDescriptor&
    operator=(const FileDescriptor&& other) = delete;


    FileDescriptor&
    operator=(const FileDescriptor& other) = delete;

    ~FileDescriptor();

    size_t
    read(void* const buf,
         size_t size);

    size_t
    pread(void* const buf,
          size_t size,
          off_t pos);

    size_t
    write(const void* const buf,
          size_t size);

    size_t
    pwrite(const void* const buf,
           size_t size,
           off_t pos);

    off_t
    seek(off_t offset,
         Whence w);

    off_t
    tell();

    void
    sync();

    uint64_t
    size();

    uint64_t
    filesystemSize();

    uint64_t
    filesystemFreeSize();

    void
    fallocate(uint64_t size);

    void
    truncate(uint64_t size);

    // Implements Lockable concept for POSIX locking
    void
    lock();

    void
    unlock();

    bool
    try_lock();

    const fs::path&
    path() const;

    void
    fadvise(FAdvise);

private:
    DECLARE_LOGGER("FileDescriptor");

    const fs::path path_;
    int fd_;
    const FDMode mode_;
    const SyncOnCloseAndDestructor sync_on_close_and_destructor_;

    // Why are these private??
    void
    stat_(struct stat& st);

    void
    statvfs_(struct statvfs& st);

    static int
    get_whence_(Whence w);
};

}
#endif // FILEDESCRIPTOR_H_
