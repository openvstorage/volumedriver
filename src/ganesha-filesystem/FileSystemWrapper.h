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

#ifndef FILE_SYSTEM_WRAPPER_H
#define FILE_SYSTEM_WRAPPER_H

#include <youtils/Assert.h>
#include <youtils/BooleanEnum.h>

#include <filesystem/FileSystem.h>
#include <filesystem/FileSystemEvents.h>

#include <boost/lexical_cast.hpp>

BOOLEAN_ENUM(LRUCloseFiles)
BOOLEAN_ENUM(LRUFreeMemory)

#define OVS_MAGIC_SIGNATURE "4F56535F636E616E616B6F73"

namespace ganesha
{

enum class LockOp
{
    Try,
    NonBlocking,
    Blocking,
    Unlock,
    Cancel
};

inline std::ostream&
operator<<(std::ostream& os,
           const LockOp& lock_op)
{
    switch(lock_op)
    {
    case LockOp::Try:
        return os << "LockOp::Try";
    case LockOp::NonBlocking:
        return os << "LockOp::NonBlocking";
    case LockOp::Blocking:
        return os << "LockOp::Blocking";
    case LockOp::Unlock:
        return os << "LockOp::Unlock";
    case LockOp::Cancel:
        return os << "LockOp::Cancel";
    default:
        UNREACHABLE;
    }

}

enum LockType
{
    Read,
    Write,
    NoLock
};

inline std::ostream&
operator<<(std::ostream& os,
           const LockType& lock_type)
{
    switch(lock_type)
    {
    case LockType::Read:
        return os << "LockType::Read";
    case LockType::Write:
        return os << "LockType::Write";
    case LockType::NoLock:
        return os << "LockType::NoLock";
    default:
        UNREACHABLE;
    }
}

enum class ObjectFileType
{
    NO_FILE_TYPE = 0,
    REGULAR_FILE = 1,
    CHARACTER_FILE = 2,
    BLOCK_FILE = 3,
    SYMBOLIC_LINK = 4,
    SOCKET_FILE = 5,
    FIFO_FILE = 6,
    DIRECTORY = 7,
    EXTENDED_ATTR = 8
};

inline std::ostream&
operator<<(std::ostream& os,
           const ObjectFileType& in)
{
    switch(in)
    {
    case ObjectFileType::NO_FILE_TYPE:
        return os << "ObjectFileType::NO_FILE_TYPE";
    case ObjectFileType::REGULAR_FILE:
        return os << "ObjectFileType::REGULAR_FILE";
    case ObjectFileType::CHARACTER_FILE:
        return os << "ObjectFileType::CHARACTER_FILE";
    case ObjectFileType::BLOCK_FILE:
        return os << "ObjectFileType::BLOCK_FILE";
    case ObjectFileType::SYMBOLIC_LINK:
        return os << "ObjectFileType::SYMBOLIC_LINK";
    case ObjectFileType::SOCKET_FILE:
        return os << "ObjectFileType::SOCKET_FILE";
    case ObjectFileType::FIFO_FILE:
        return os << "ObjectFileType::FIFO_FILE";
    case ObjectFileType::DIRECTORY:
        return os << "ObjectFileType::DIRECTORY";
    case ObjectFileType::EXTENDED_ATTR:
        return os << "ObjectFileType::EXTENDED_ATTR";
    }
    UNREACHABLE;
}

class Attribute
{};

typedef std::list<Attribute> Attributes;

/* Need to find out how this is handled. */
typedef uint16_t OpenFlags;

struct FileSystemWrapperException : public std::exception
{
    FileSystemWrapperException(uint64_t error);

    ~FileSystemWrapperException() noexcept;

    virtual const char* what() const noexcept;

    template<typename T>
    FileSystemWrapperException&
    operator<<(const T& t)
    {
        std::stringstream tmp;

        tmp << t;
        what_ += tmp.str();
        return *this;
    }

    std::string what_;
    uint64_t error_;
};

/* hide stat and make FileSystemWrapper a friend. */
struct FSData
{
public:
    explicit FSData(volumedriverfs::FileSystem* fs,
                    const struct stat& stat)
        : stat_(stat)
        , handle_(nullptr)
        , mode_(0)
        , fs_(fs)
    {}

    FSData(volumedriverfs::FileSystem* fs,
           const volumedriverfs::FrontendPath& p)
        : handle_(nullptr)
        , mode_(0)
        , fs_(fs)
    {
        fs_->getattr(p, stat_);
    }

    FSData(volumedriverfs::FileSystem* fs,
           const volumedriverfs::ObjectId& id)
        : handle_(nullptr)
        , mode_(0)
        , fs_(fs)
    {
        fs_->getattr(id, stat_);
    }

    FSData(const FSData& in)
        : stat_(in.stat_)
        , handle_(in.handle_)
        , mode_(in.mode_)
        , fs_(in.fs_)
    {}

    FSData&
    operator=(const FSData&) = delete;

    FSData(FSData&& in)
        : stat_(in.stat_)
        , handle_(in.handle_)
        , mode_(in.mode_)
        , fs_(in.fs_)
    {
        in.handle_ = nullptr;
    }

    DECLARE_LOGGER("FSData");

    ~FSData() = default;

    ObjectFileType
    objectFileType() const;

    struct stat stat_;

    typedef std::shared_ptr<volumedriverfs::Handle> HandlePtr;
    HandlePtr handle_;

    mode_t mode_;
    volumedriverfs::FileSystem* fs_;
};

template<uint64_t err_code>
struct FileSystemWrapperExceptionT  : public FileSystemWrapperException
{
    FileSystemWrapperExceptionT()
        : FileSystemWrapperException(err_code)
    {}

    ~FileSystemWrapperExceptionT()
    {}
};

class FileSystemWrapper
{
public:
    typedef std::vector< std::string> directory_entries_t;

    FileSystemWrapper(const std::string& export_path,
                      const std::string& configuration_file);

    ~FileSystemWrapper();

    void
    lookup(const volumedriverfs::ObjectId& parent_id,
           const std::string& name,
           boost::optional<volumedriverfs::ObjectId>& id,
           struct stat& st);

    void
    create(const volumedriverfs::ObjectId& parent_id,
           const std::string& name,
           volumedriverfs::UserId uid,
           volumedriverfs::GroupId gid,
           volumedriverfs::Permissions pms,
           boost::optional<volumedriverfs::ObjectId>& id,
           struct stat& st);

    void
    makedir(const volumedriverfs::ObjectId& parent_id,
            const std::string& name,
            volumedriverfs::UserId uid,
            volumedriverfs::GroupId gid,
            volumedriverfs::Permissions pms,
            boost::optional<volumedriverfs::ObjectId>& id,
            struct stat& st);

    void
    read_dirents(const volumedriverfs::ObjectId& dir_id,
                 directory_entries_t& dirents,
                 const size_t start);

    void
    rename_file(const volumedriverfs::ObjectId& oldparent,
                const std::string& old_name,
                const volumedriverfs::ObjectId& newparent,
                const std::string& new_name);

    void
    getattrs(const volumedriverfs::ObjectId& id,
             struct stat& stat);

    void
    utimes(const volumedriverfs::ObjectId& id,
           const struct timespec timebuf[2]);

    // void
    // setattrs(const volumedriverfs::ObjectId& entity,
    //          const Attributes& attrs);

    void
    truncate(const volumedriverfs::ObjectId& id,
             off_t size);

    void
    file_unlink(const volumedriverfs::ObjectId& parent_id,
                const char* name);

    void
    lookup_path(const std::string& path,
                FSData& fsdata,
                boost::optional<volumedriverfs::ObjectId>& id,
                struct stat& st);

    ObjectFileType
    file_type_from_inode(const FSData& fsdata);

    void
    open(const volumedriverfs::ObjectId& id,
         const mode_t mode, FSData& fsdata);

    OpenFlags
    status(const FSData& fsdata);

    void
    read(const FSData& fsdata,
         const uint64_t offset,
         const size_t buffer_size,
         void* buffer,
         size_t& read_amount,
         bool& eof);

    void
    write(const FSData& fsdata,
          const uint64_t offset,
          const size_t buffer_size,
          const void* buffer,
          size_t& write_amount,
          bool& fsal_stable);

    void
    commit(const FSData& fsdata,
           const off_t offset,
           const size_t len);

    void
    close(FSData& fsdata);

    void
    release(const volumedriverfs::ObjectId& id);

    void
    chmod(const volumedriverfs::ObjectId& id,
          const mode_t mode);

    void
    chown(const volumedriverfs::ObjectId& id,
          const uid_t uid,
          const gid_t gid);

    void
    statvfs(struct statvfs& vfs);

    /*
    void
    lock_op(const volumedriverfs::ObjectId& entity,
            void* p_owner,
            fsal_lock_op_t lock_op,
            fsal_lock_param_t* request_lock,
            fsal_lock_param_t* conflicting_lock);
    */

    void
    lru_cleanup(const volumedriverfs::ObjectId& id,
                const LRUCloseFiles,
                const LRUFreeMemory);

    void
    up_and_running(const std::string mountpoint)
    {
        const auto ev(volumedriverfs::FileSystemEvents::up_and_running(mountpoint));
        fs_->object_router().event_publisher()->publish(ev);
    }

    std::vector<std::string>
    ovs_discovery_message()
    {
        std::vector<std::string> vec;
        const std::string signature(OVS_MAGIC_SIGNATURE);
        const volumedriverfs::ClusterNodeConfig nc(fs_->object_router().node_config());
        vec.push_back(signature);
        vec.push_back(boost::lexical_cast<std::string>(static_cast<int>(nc.xmlrpc_port)));
        vec.push_back(std::string(fs_->object_router().cluster_id()));
        return vec;
    }
private:
    DECLARE_LOGGER("FileSystemWrapper");

    const std::string export_path_;

    std::unique_ptr<volumedriverfs::FileSystem> fs_;

    volumedriverfs::FrontendPath
    normalize_path(const volumedriverfs::FrontendPath& path) const;

};

}

#endif
