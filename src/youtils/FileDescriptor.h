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

#ifndef FILEDESCRIPTOR_H_
#define FILEDESCRIPTOR_H_

#include <boost/filesystem.hpp>
#include "BooleanEnum.h"
#include "Logging.h"

BOOLEAN_ENUM(CreateIfNecessary);
BOOLEAN_ENUM(SyncOnCloseAndDestructor);
struct statvfs;
struct stat;

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


class FileDescriptor
{
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
