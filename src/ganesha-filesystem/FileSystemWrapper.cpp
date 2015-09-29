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

#include "FileSystemWrapper.h"

#include <boost/filesystem/path.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <youtils/VolumeDriverComponent.h>

namespace ganesha
{

namespace fs = boost::filesystem;
namespace vfs = volumedriverfs;

namespace
{

const vfs::FrontendPath root("/");
const fs::path dot = { "." };
const fs::path dotdot = { ".." };

struct HandleDeleter
{
    HandleDeleter(vfs::FileSystem& fs)
        : fs_(fs)
    {}

    void
    operator()(vfs::Handle* h)
    {
        fs_.release(vfs::Handle::Ptr(h));
    }

    vfs::FileSystem& fs_;
};

} // namespace

FileSystemWrapperException::FileSystemWrapperException(uint64_t error)
    : error_(error)
{}

FileSystemWrapperException::~FileSystemWrapperException() noexcept
{}

const char*
FileSystemWrapperException::what() const noexcept
{
    return what_.c_str();
}

using namespace volumedriverfs;

typedef FileSystemWrapperExceptionT<1> ErrorPERM;
typedef FileSystemWrapperExceptionT<2> ErrorNOENT;
typedef FileSystemWrapperExceptionT<5> ErrorIO;
typedef FileSystemWrapperExceptionT<6> ErrorNXIO;
typedef FileSystemWrapperExceptionT<12> ErrorNOMEM;
typedef FileSystemWrapperExceptionT<13> ErrorACCESS;
typedef FileSystemWrapperExceptionT<14> ErrorFAULT;
typedef FileSystemWrapperExceptionT<17> ErrorEXIST;
typedef FileSystemWrapperExceptionT<18> ErrorXDEV;
typedef FileSystemWrapperExceptionT<20> ErrorNOTDIR;
typedef FileSystemWrapperExceptionT<21> ErrorISDIR;
typedef FileSystemWrapperExceptionT<22> ErrorINVAL;
typedef FileSystemWrapperExceptionT<27> ErrorFBIG;
typedef FileSystemWrapperExceptionT<28> ErrorNOSPC;
typedef FileSystemWrapperExceptionT<30> ErrorROFS;
typedef FileSystemWrapperExceptionT<31> ErrorMLINK;
typedef FileSystemWrapperExceptionT<49> ErrorDQUOT;
typedef FileSystemWrapperExceptionT<78> ErrorNAMETOOLONG;
typedef FileSystemWrapperExceptionT<93> ErrorNOTEMPTY;
typedef FileSystemWrapperExceptionT<151> ErrorSTALE;
typedef FileSystemWrapperExceptionT<10001> ErrorBADHANDLE;
typedef FileSystemWrapperExceptionT<10003> ErrorBADCOOKIE;
typedef FileSystemWrapperExceptionT<10004> ErrorNOTSUPP;
typedef FileSystemWrapperExceptionT<10005> ErrorTOOSMALL;
typedef FileSystemWrapperExceptionT<10006> ErrorSERVERFAULT;
typedef FileSystemWrapperExceptionT<10007> ErrorBADTYPE;
typedef FileSystemWrapperExceptionT<10008> ErrorDELAY;
typedef FileSystemWrapperExceptionT<10014> ErrorFHEXPIRED;
typedef FileSystemWrapperExceptionT<10015> ErrorSHARE_DENIED;
typedef FileSystemWrapperExceptionT<10029> ErrorSYMLINK;
typedef FileSystemWrapperExceptionT<10032> ErrorATTRNOTSUPP;
typedef FileSystemWrapperExceptionT<20001> ErrorNOT_INIT;
typedef FileSystemWrapperExceptionT<20002> ErrorALREADY_INIT;
typedef FileSystemWrapperExceptionT<20003> ErrorBAD_INIT;
typedef FileSystemWrapperExceptionT<20004> ErrorSEC;
typedef FileSystemWrapperExceptionT<20005> ErrorNO_QUOTA;
typedef FileSystemWrapperExceptionT<20010> ErrorNOT_OPENED;
typedef FileSystemWrapperExceptionT<20011> ErrorDEADLOCK;
typedef FileSystemWrapperExceptionT<20012> ErrorOVERFLOW;
typedef FileSystemWrapperExceptionT<20013> ErrorINTERRUPT;
typedef FileSystemWrapperExceptionT<20014> ErrorBLOCKED;
typedef FileSystemWrapperExceptionT<20015> ErrorTIMEOUT;
typedef FileSystemWrapperExceptionT<10046> ErrorFILE_OPEN;

ObjectFileType
FSData::objectFileType() const
{
    if(S_ISFIFO(stat_.st_mode))
    {
        return ObjectFileType::FIFO_FILE;
    }
    else if(S_ISCHR(stat_.st_mode))
    {
        return ObjectFileType::CHARACTER_FILE;
    }
    else if(S_ISDIR(stat_.st_mode))
    {
        return ObjectFileType::DIRECTORY;
    }
    else if(S_ISBLK(stat_.st_mode))
    {
        return ObjectFileType::BLOCK_FILE;
    }
    else if(S_ISREG(stat_.st_mode))
    {
        return ObjectFileType::REGULAR_FILE;
    }
    else if(S_ISLNK(stat_.st_mode))
    {
        return ObjectFileType::SYMBOLIC_LINK;
    }
    else if(S_ISSOCK(stat_.st_mode))
    {
        return ObjectFileType::SOCKET_FILE;
    }
    else
    {
        throw ErrorINVAL() << "Cannot determine file type of " << stat_.st_mode;
    }
}

FileSystemWrapper::FileSystemWrapper(const std::string& export_path,
                                     const std::string& configuration_file)
    : export_path_(export_path)
{
    boost::property_tree::ptree configuration_ptree;
    boost::property_tree::json_parser::read_json(configuration_file, configuration_ptree);
    youtils::VolumeDriverComponent::verify_property_tree(configuration_ptree);

    fs_.reset(new FileSystem(configuration_ptree));
}

FileSystemWrapper::~FileSystemWrapper()
{
    fs_.reset(nullptr);
}

vfs::FrontendPath
FileSystemWrapper::normalize_path(const vfs::FrontendPath& in_path) const
{
    ASSERT(in_path.has_root_directory());

    vfs::FrontendPath out;
    for(const auto& i : in_path)
    {
        if(i == dot)
        {
        }

        else if(i == dotdot)
        {
            if(not  out.relative_path().empty())
            {
                out.remove_filename();
            }

        }
        else
        {
            out /= i;
        }
    }

    return out;
}

void
FileSystemWrapper::lookup(const vfs::ObjectId& parent_id,
                          const std::string& name,
                          boost::optional<vfs::ObjectId>& id,
                          struct stat& st)
{
    const vfs::FrontendPath fpath(fs_->find_path(parent_id) / name);

    FSData fsdata(fs_.get(), fpath);
    id = fs_->find_id(fpath);
    st = fsdata.stat_;
}

void
FileSystemWrapper::create(const vfs::ObjectId& parent_id,
                          const std::string& name,
                          vfs::UserId uid,
                          vfs::GroupId gid,
                          vfs::Permissions pms,
                          boost::optional<vfs::ObjectId>& id,
                          struct stat& st)
{
    fs_->mknod(parent_id,
               name,
               uid,
               gid,
               pms);

    const FrontendPath fpath(fs_->find_path(parent_id) / name);

    FSData fsdata(fs_.get(), fpath);
    id = fs_->find_id(fpath);
    st = fsdata.stat_;
}

void
FileSystemWrapper::makedir(const volumedriverfs::ObjectId& parent_id,
                           const std::string& name,
                           volumedriverfs::UserId uid,
                           volumedriverfs::GroupId gid,
                           volumedriverfs::Permissions pms,
                           boost::optional<volumedriverfs::ObjectId>& id,
                           struct stat& st)
{
    fs_->mkdir(parent_id,
               name,
               uid,
               gid,
               pms);

    const vfs::FrontendPath fpath(fs_->find_path(parent_id) / name);

    FSData fsdata(fs_.get(), fpath);
    id = fs_->find_id(fpath);
    st = fsdata.stat_;
}

void
FileSystemWrapper::read_dirents(const vfs::ObjectId& dir_id,
                                directory_entries_t& dirents,
                                size_t start)
{
    fs_->read_dirents(dir_id,
                      dirents,
                      start);
}

void
FileSystemWrapper::rename_file(const vfs::ObjectId& oldparent,
                               const std::string& old_name,
                               const vfs::ObjectId& newparent,
                               const std::string& new_name)
{
    /* If src and dst refer to the same file then according
     * to POSIX and NFS3/4, Ganesha should do nothing and
     * return success.
     */
    fs_->rename(oldparent,
                old_name,
                newparent,
                new_name);
}

void
FileSystemWrapper::getattrs(const vfs::ObjectId& id,
                            struct stat& stat)
{
    fs_->getattr(id,
                 stat);
}

void
FileSystemWrapper::release(const vfs::ObjectId& /*id*/)
{
}

void
FileSystemWrapper::truncate(const vfs::ObjectId& id,
                            off_t size)
{
    fs_->truncate(id,
                  size);
}

void
FileSystemWrapper::file_unlink(const vfs::ObjectId& parent_id,
                               const char* name)
{
    fs_->unlink(parent_id,
                name);
}

void
FileSystemWrapper::lookup_path(const std::string& path,
                               FSData& fsdata,
                               boost::optional<vfs::ObjectId>& id,
                               struct stat& st)
{
    LOG_TRACE("path " << path);
    assert(path.find(export_path_) == 0);
    vfs::FrontendPath fpath(path.substr(export_path_.size()));
    if(not fpath.empty())
    {
        fpath = normalize_path(fpath);
    }
    else
    {
        fpath = root;
    }

    fsdata.fs_ = fs_.get();
    fs_->getattr(fpath,
                 fsdata.stat_);
    fsdata.mode_ = 0;

    id = fs_->find_id(fpath);
    st = fsdata.stat_;

    LOG_TRACE("Normalized Path " << fpath << ", id: " << id);
}

ObjectFileType
FileSystemWrapper::file_type_from_inode(const FSData& fsdata)
{
    if(fsdata.handle_)
    {
        return fsdata.objectFileType();
    }
    else
    {
        throw ErrorNOENT() << " no hadle error";
    }
}

void
FileSystemWrapper::chmod(const vfs::ObjectId& id,
                         const mode_t mode)
{
    fs_->chmod(id,
               mode);
}

void
FileSystemWrapper::chown(const vfs::ObjectId& id,
                         const uid_t uid,
                         const gid_t gid)
{
    fs_->chown(id,
               uid,
               gid);
}

void
FileSystemWrapper::utimes(const vfs::ObjectId& id,
                          const struct timespec timebuf[2])
{
    fs_->utimens(id,
                 timebuf);
}

void
FileSystemWrapper::open(const vfs::ObjectId& id,
                        const mode_t mode, FSData& fsdata)
{
    if (fsdata.handle_)
    {
        return;
    }
    else
    {
        vfs::Handle::Ptr h;
        fs_->open(id,
                  mode,
                  h);
        //fsdata.fs_ = fs_.get();
        fsdata.handle_ = std::shared_ptr<vfs::Handle>(h.release(),
                                                      HandleDeleter(*fs_));
        fsdata.mode_ = mode;
    }
}

OpenFlags
FileSystemWrapper::status(const FSData& fsdata)
{
    static const uint16_t FSAL_O_CLOSED = 0x0000;

    if(fsdata.handle_)
    {
        /* Everything is open for read and write... */
        uint16_t FSAL_O_RDWR = 0x0001 bitor 0x0002;
        return FSAL_O_RDWR;
    }
    else
    {
        return FSAL_O_CLOSED;
    }
}

void
FileSystemWrapper::read(const FSData& fsdata,
                        const uint64_t offset,
                        const size_t buffer_size,
                        void* buffer,
                        size_t& read_amount,
                        bool& eof)
{
    /* Can we get a read without an open? */
    if(not fsdata.handle_)
    {
        throw ErrorSERVERFAULT() << " no handle found";
    }
    else
    {
        read_amount = buffer_size;
        fs_->read(*fsdata.handle_,
                  read_amount,
                  static_cast<char*>(buffer),
                  offset,
                  eof);
    }
}

void
FileSystemWrapper::write(const FSData& fsdata,
                         const uint64_t offset,
                         const size_t buffer_size,
                         const void* buffer,
                         size_t& write_amount,
                         bool& fsal_stable)
{
    /* Can we get a write without an open? */
    if(not fsdata.handle_)
    {
        throw ErrorSERVERFAULT() << " no handle found ";
    }
    else
    {
        write_amount = buffer_size;
        fs_->write(*fsdata.handle_,
                   write_amount,
                   static_cast<const char*>(buffer),
                   offset,
                   fsal_stable);
    }
}

void
FileSystemWrapper::commit(const FSData& fsdata,
                          const off_t /*offset*/,
                          const size_t /*len*/)
{
        if(not fsdata.handle_)
        {
            throw ErrorSERVERFAULT() << " no handle found ";
        }
        else
        {
            fs_->fsync(*fsdata.handle_,
                       false);
        }
}

void
FileSystemWrapper::close(FSData& fsdata)
{
    if(fsdata.handle_)
    {
        fsdata.handle_ = nullptr;
    }
}

void
FileSystemWrapper::lru_cleanup(const vfs::ObjectId& /*id*/,
                               const LRUCloseFiles,
                               const LRUFreeMemory)
{}

void
FileSystemWrapper::statvfs(struct statvfs& st)
{
    LOG_TRACE("statvfs'ing");

    fs_->statfs(root, st);
}

} //namespace ganesha
