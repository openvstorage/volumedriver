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

#include <youtils/Assert.h>
#include "FileSystem.h"
#include <youtils/Logging.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>

#include <boost/foreach.hpp>
#include <boost/scope_exit.hpp>
#include <boost/thread.hpp>

#include <fuse/fuse_lowlevel.h>

#define LOCK()                                  \
    std::lock_guard<lock_type> g__(lock_)


namespace fawltyfs
{

namespace fs = boost::filesystem;

const std::vector<std::string>
FileSystem::default_fuse_args =
{
    "-f",
    "-s"
};

FileSystem::FileSystem(const fs::path& backend_dir,
                       const fs::path& frontend_dir,
                       const std::vector<std::string>& fuse_args,
                       const char* fsname)
    : name_(fsname)
    , backend_dir_(backend_dir)
    , frontend_dir_(frontend_dir)
    , fuse_args_(fuse_args)
    , fuse_(0)
{
    LOG_INFO(name_ << " initialized: backend directory " << backend_dir_ <<
             ", frontend directory " << frontend_dir_);
}

FileSystem::~FileSystem()
{
    if(thread_)
    {
        LOG_WARN(name_ <<
                 " seems to be still running - waiting for it to finish");
        try
        {
            thread_->join();
        }
        catch (std::exception& e)
        {
            LOG_ERROR(name_ << ": caught exception: " << e.what() <<
                      " - ignoring it.");
        }
        catch (...)
        {
            LOG_ERROR(name_ << ": caught unknown exception - ignoring it");
        }
     }
    LOG_INFO("Sayonara");
}

void
FileSystem::init_ops_(fuse_operations& ops) const
{
    bzero(&ops, sizeof(ops));

#define INSTALL_CB(name)                        \
    ops.name = FileSystem::name

    INSTALL_CB(getattr);
    INSTALL_CB(access);
    INSTALL_CB(readlink);
    INSTALL_CB(readdir);
    INSTALL_CB(mkdir);
    INSTALL_CB(unlink);
    INSTALL_CB(rmdir);
    INSTALL_CB(rename);
    INSTALL_CB(link);
    INSTALL_CB(symlink);
    INSTALL_CB(chmod);
    INSTALL_CB(chown);
    INSTALL_CB(truncate);
    INSTALL_CB(ftruncate);
    INSTALL_CB(utimens);
    INSTALL_CB(open);
    INSTALL_CB(read);
    INSTALL_CB(write);
    INSTALL_CB(statfs);
    INSTALL_CB(release);
    INSTALL_CB(fsync);
    INSTALL_CB(mknod);
    INSTALL_CB(opendir);
    INSTALL_CB(releasedir);

#undef INSTALL_CB
}

void
FileSystem::run_(bool mt, char* mountpoint)
{
    ASSERT(fuse_);

    BOOST_SCOPE_EXIT((&fuse_)(mountpoint))
    {
        fuse_teardown(fuse_, mountpoint);
        fuse_ = 0;
    }
    BOOST_SCOPE_EXIT_END;

    const int res = mt ? fuse_loop_mt(fuse_) : fuse_loop(fuse_);
    if (res != 0)
    {
        LOG_ERROR("fuse loop exited with status " << res);
    }
    else
    {
        LOG_INFO("fuse loop exited");
    }
}

void
FileSystem::mount()
{
    LOG_INFO(name_ << ": mounting");

    LOCK();

    if (thread_)
    {
        LOG_ERROR(name_ << ": already mounted?");
        throw Exception(name_ + std::string(": already mounted"));
    }

    std::vector<char*> fuse_argv(fuse_args_.size() + 2);

    BOOST_SCOPE_EXIT((&fuse_argv))
    {
        BOOST_FOREACH(char* str, fuse_argv)
        {
            free(str);
        }
    }
    BOOST_SCOPE_EXIT_END;

    fuse_argv[0] = strdup(name_.c_str());
    for (size_t i = 0; i < fuse_args_.size(); ++i)
    {
        LOG_INFO((i + 1) << ": " << fuse_args_[i]);
        fuse_argv[i + 1] = ::strdup(fuse_args_[i].c_str());
    }

    fuse_argv[fuse_args_.size() + 1] = ::strdup(frontend_dir_.string().c_str());

    fuse_operations ops;
    init_ops_(ops);

    // The following is based on fuse_main_common. fuse_setup is done in the context
    // of the caller so any errors parsing the arguments etc. can be reported.
    // Only once that succeeded a thread is spawned to run the fuse event loop
    // - it also has to do the clean up (free(mountpoint) etc. in fuse_teardown).
    // Yes, this and in particular the back and forth of the mountpoint param is ugly
    // as sin - to get rid of this (and the string params in general, while at it)
    // fuse_setup and fuse_teardown need to be dissected - be my guest.

    char *mountpoint;
    int multithreaded;
    fuse_ = fuse_setup(fuse_argv.size(),
                       &fuse_argv[0],
                       &ops,
                       sizeof(ops),
                       &mountpoint,
                       &multithreaded,
                       this);
    if (not fuse_)
    {
        LOG_ERROR(name_ << ": fuse_setup_common failed");
        throw Exception("problem running filesystem");
    }

    thread_.reset(new std::thread(std::bind(&FileSystem::run_,
                                            this,
                                            multithreaded,
                                            mountpoint)));
}

void
FileSystem::umount()
{
    {
        LOCK();

        LOG_INFO(name_ << ": umounting");
        if(thread_)
        {
            fuse_unmount(frontend_dir_.string().c_str(), 0);

            // fuse_unmount leads to a mysterious segfault when using pyfawltyfs with
            // ipython, so let's go call fusermount -u instead. This has the added benefit
            // of error reporting.


            // pid_t pid = ::fork();
            // if (pid < 0)
            // {
            //     std::stringstream ss;
            //     ss << "Failed to spawn process: " << strerror(errno);
            //     LOG_ERROR(ss.str());
            //     throw Exception(ss.str());
            // }
            // else if (pid > 0)
            // {
            //     int status;
            //     ::waitpid(pid, &status, 0);
            //     if (status != 0)
            //     {
            //         std::stringstream ss;
            //         ss << "failed to umount - fusermount returned status " << status;
            //         LOG_ERROR(ss.str());
            //         throw Exception(ss.str());
            //     }
            // }
            // else
            // {
            //     const int ret = ::execlp("fusermount",
            //                              "fusermount",
            //                              "-u",
            //                              frontend_dir_.string().c_str(),
            //                              static_cast<const char*>(0));
            //     if (ret < 0)
            //     {
            //         LOG_ERROR("failed to exec fusermount: " << strerror(errno));
            //         exit(-ret);
            //     }
            //     else
            //     {
            //         exit(ret);
            //     }
            // }
        }
    }

    LOG_INFO(name_ << ": umounted - stopping thread");
    thread_->join();
    thread_.reset();
}

bool
FileSystem::mounted() const
{
    LOCK();

    if (fuse_)
    {
        return not fuse_session_exited(fuse_get_session(fuse_));
    }
    else
    {
        return false;
    }
}

#define CONVERT_EXCEPTIONS(x)                                           \
    try                                                                 \
    {                                                                   \
        auto r = x;                                                     \
        return r;                                                       \
    }                                                                   \
    catch (std::exception& e)                                           \
    {                                                                   \
        LOG_ERROR("Caught exception " << e.what() << ": returning I/O error"); \
        LOG_TRACE("EXITED " #x ", retval -EIO");                          \
        return -EIO;                                                    \
    }                                                                   \
    catch (...)                                                         \
    {                                                                   \
        LOG_ERROR("Caught unknown exception: returning I/O error");     \
        LOG_TRACE("EXITED " #x ", retval -EIO");                        \
        return -EIO;                                                    \
    }                                                                   \

int
FileSystem::getattr(const char* path,
                    struct stat* st)
{
    CONVERT_EXCEPTIONS(instance_()->getattr_(path, st));
}

int
FileSystem::access(const char* path,
                   int mask)
{
    CONVERT_EXCEPTIONS(instance_()->access_(path, mask));
}

int
FileSystem::readlink(const char* path,
                     char* buf,
                     size_t size)
{
    CONVERT_EXCEPTIONS(instance_()->readlink_(path, buf, size));
}

int
FileSystem::opendir(const char* path,
                    fuse_file_info* fi)
{
    CONVERT_EXCEPTIONS(instance_()->opendir_(path, fi));
}

int
FileSystem::releasedir(const char* path,
                       fuse_file_info* fi)
{
    CONVERT_EXCEPTIONS(instance_()->releasedir_(path, fi));
}

int
FileSystem::readdir(const char* path,
                    void* buf,
                    fuse_fill_dir_t filler,
                    off_t offset,
                    struct fuse_file_info* fi)
{
    CONVERT_EXCEPTIONS(instance_()->readdir_(path, buf, filler, offset, fi));
}

int
FileSystem::mknod(const char* path,
                  mode_t mode,
                  dev_t rdev)
{
    CONVERT_EXCEPTIONS(instance_()->mknod_(path, mode, rdev));
}

int
FileSystem::mkdir(const char* path,
                  mode_t mode)
{
    CONVERT_EXCEPTIONS(instance_()->mkdir_(path, mode));
}

int
FileSystem::unlink(const char* path)
{
    CONVERT_EXCEPTIONS(instance_()->unlink_(path));
}

int
FileSystem::rmdir(const char* path)
{
    CONVERT_EXCEPTIONS(instance_()->rmdir_(path));
}

int
FileSystem::symlink(const char* from,
                    const char* to)
{
    CONVERT_EXCEPTIONS(instance_()->symlink_(from, to));
}

int
FileSystem::rename(const char* from,
                   const char* to)
{
    CONVERT_EXCEPTIONS(instance_()->rename_(from, to));
}

int
FileSystem::link(const char* from,
                 const char* to)
{
    CONVERT_EXCEPTIONS(instance_()->link_(from, to));
}

int
FileSystem::chmod(const char* path, mode_t mode)
{
    CONVERT_EXCEPTIONS(instance_()->chmod_(path, mode));
}

int
FileSystem::chown(const char* path,
                  uid_t uid,
                  gid_t gid)
{
    CONVERT_EXCEPTIONS(instance_()->chown_(path, uid, gid));
}

int
FileSystem::truncate(const char* path,
                     off_t size)
{
    CONVERT_EXCEPTIONS(instance_()->truncate_(path, size));
}

int
FileSystem::ftruncate(const char* path,
                      off_t size,
                      fuse_file_info* fi)
{
    CONVERT_EXCEPTIONS(instance_()->ftruncate_(path, size, fi));
}

int
FileSystem::utimens(const char* path,
                    const struct timespec ts[2])
{
    CONVERT_EXCEPTIONS(instance_()->utimens_(path, ts));
}

int
FileSystem::open(const char* path,
                 struct fuse_file_info* fi)
{
    CONVERT_EXCEPTIONS(instance_()->open_(path, fi));
}

int
FileSystem::read(const char* path,
                 char* buf,
                 size_t size,
                 off_t off,
                 struct fuse_file_info* fi)
{
    CONVERT_EXCEPTIONS(instance_()->read_(path, buf, size, off, fi));
}

int
FileSystem::write(const char* path,
                  const char* buf,
                  size_t size,
                  off_t off,
                  struct fuse_file_info* fi)
{
    CONVERT_EXCEPTIONS(instance_()->write_(path, buf, size, off, fi));
}

int
FileSystem::statfs(const char* path,
                   struct statvfs* stbuf)
{
    CONVERT_EXCEPTIONS(instance_()->statfs_(path, stbuf));
}

int
FileSystem::release(const char* path,
                    struct fuse_file_info* fi)
{
    CONVERT_EXCEPTIONS(instance_()->release_(path, fi));
}

int
FileSystem::fsync(const char* path,
                  int datasync,
                  struct fuse_file_info* fi)
{
    CONVERT_EXCEPTIONS(instance_()->fsync_(path, datasync, fi));
}

#undef CONVERT_EXCEPTIONS

int
FileSystem::getattr_(const std::string& path,
                     struct stat *stbuf)
{
    LOG_TRACE("getattr " << path);

    int res = maybeInjectError_(path, FileSystemCall::Getattr);
    if (res == 0)
    {
        const fs::path p(backend_dir_ / path);
        res = ::lstat(p.string().c_str(), stbuf);
        if (res == -1)
        {
            res = -errno;
            // ENOENT does not necessarily indicate an error on the underlying
            // filesystem here, as FUSE probing for the existence of files /
            // directories ends up here.
            if (res == -ENOENT)
            {
                LOG_TRACE(p << ": " << strerror(-res));
            }
            else
            {
                LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                          strerror(-res));
            }
        }
    }

    return res;
}

int
FileSystem::access_(const std::string& path,
                    int mask)
{
    LOG_TRACE("access " << path << ", mask " << mask);

    int res = maybeInjectError_(path, FileSystemCall::Access);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);
    res = ::access(p.string().c_str(), mask);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::readlink_(const std::string& path,
                      char *buf,
                      size_t size)
{
    LOG_TRACE("readlink " << path << ", bufsize " << size);

    int res = maybeInjectError_(path, FileSystemCall::Readlink);
    if (res == 0)
    {
        const fs::path p(backend_dir_ / path);
        std::vector<char> tmp(size + backend_dir_.string().size() + 1);
        ssize_t ret = ::readlink(p.string().c_str(), &tmp[0], tmp.size());
        if (ret == -1)
        {
            res = -errno;
            LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                      strerror(-res));
        }
        else
        {
            const fs::path q(std::string(&tmp[0], tmp.size()).substr(backend_dir_.string().size()));
            VERIFY(static_cast<size_t>(ret) >= q.string().size());
            ret -= backend_dir_.string().size();
            memcpy(buf, q.string().c_str(),
                   std::min(q.string().size() + 1,
                            size));
            buf[ret] = '\0';
            LOG_TRACE("path: " << path <<
                      ", base dir: " << backend_dir_.string() <<
                      ", read link: " << q << ", buf: " << buf <<
                      ", readlink ret: " << ret);
            res = 0;
        }
    }

    return res;
}

int
FileSystem::opendir_(const std::string& path,
                     fuse_file_info* /* fi */)
{
    LOG_TRACE("opendir " << path);

    int res = maybeInjectError_(path, FileSystemCall::Opendir);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);
    DIR* dir = ::opendir(p.string().c_str());
    if (dir == 0)
    {
        res = -errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return res;
    }

    ::closedir(dir);
    return 0;
}

int
FileSystem::releasedir_(const std::string& path,
                        fuse_file_info* /* fi */)
{
    LOG_TRACE("releasedir " << path);

    int res = maybeInjectError_(path, FileSystemCall::Opendir);
    if (res != 0)
    {
        return res;
    }

    // not implemented since we don't keep the directory open
    return 0;
}

int
FileSystem::readdir_(const std::string& path,
                     void *buf,
                     fuse_fill_dir_t filler,
                     off_t /* offset */,
                     struct fuse_file_info* /* fi */)
{
    LOG_TRACE("readdir " << path);

    int res = maybeInjectError_(path, FileSystemCall::Readdir);
    if (res != 0)
    {
        return res;
    }

    DIR *dp;
    struct dirent *de;

    const fs::path p(backend_dir_ / path);
    dp = ::opendir(p.string().c_str());
    if (dp == NULL)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    while ((de = ::readdir(dp)) != NULL)
    {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0))
        {
            break;
        }
    }

    ::closedir(dp);
    return 0;
}

int
FileSystem::mknod_(const std::string& path,
                   mode_t mode,
                   dev_t rdev)
{
    LOG_TRACE("mknod " << path << ", mode " << mode << ", rdev " << rdev);

    int res = maybeInjectError_(path, FileSystemCall::Mknod);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);

    /* On Linux this could just be 'mknod(path, mode, rdev)' but this
       is more portable */
    if (S_ISREG(mode)) {
        res = ::open(p.string().c_str(),
                     O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res < 0)
        {
            res = errno;
            LOG_ERROR(p <<
                      ": (open) encountered error on underlying filesystem: " <<
                      strerror(res));
            return -res;
        }
        res = ::close(res);
    } else if (S_ISFIFO(mode))
        res = ::mkfifo(p.string().c_str(), mode);
    else
        res = ::mknod(p.string().c_str(), mode, rdev);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(path << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::mkdir_(const std::string& path,
                   mode_t mode)
{
    LOG_TRACE("mkdir " << path << ", mode " << mode);

    int res = maybeInjectError_(path, FileSystemCall::Mkdir);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);

    res = ::mkdir(p.string().c_str(), mode);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::unlink_(const std::string& path)
{
    LOG_TRACE("unlink " << path);

    int res = maybeInjectError_(path, FileSystemCall::Unlink);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);
    res = ::unlink(p.string().c_str());
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::rmdir_(const std::string& path)
{
    LOG_TRACE("rmdir " << path);

    int res = maybeInjectError_(path, FileSystemCall::Rmdir);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);
    res = ::rmdir(p.string().c_str());
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::symlink_(const std::string& from,
                     const std::string& to)
{
    LOG_TRACE("symlink from " << from << " -> " << to);

    int res = maybeInjectError_(from, FileSystemCall::Symlink);
    if (res != 0)
    {
        return res;
    }

    res = maybeInjectError_(to, FileSystemCall::Symlink);
    if (res != 0)
    {
        return res;
    }

    const fs::path f(backend_dir_ / from);
    const fs::path t(backend_dir_ / to);

    res = ::symlink(f.string().c_str(), t.string().c_str());
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(f << ", " << t <<
                  ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::rename_(const std::string& from,
                    const std::string& to)
{
    LOG_TRACE("rename " << from << " -> " << to);

    int res = maybeInjectError_(from, FileSystemCall::Rename);
    if (res != 0)
    {
        return res;
    }

    res = maybeInjectError_(to, FileSystemCall::Rename);
    if (res != 0)
    {
        return res;
    }

    const fs::path f(backend_dir_ / from);
    const fs::path t(backend_dir_ / to);

    res = ::rename(f.string().c_str(), t.string().c_str());
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(f << ", " << t <<
                  ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::link_(const std::string& from,
                  const std::string& to)
{
    LOG_TRACE("link " << from << " -> " << to);

    int res = maybeInjectError_(from, FileSystemCall::Link);
    if (res != 0)
    {
        return res;
    }

    res = maybeInjectError_(to, FileSystemCall::Link);
    if (res != 0)
    {
        return res;
    }

    const fs::path f(backend_dir_ / from);
    const fs::path t(backend_dir_ / to);

    res = ::link(f.string().c_str(), t.string().c_str());
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(f << ", " << t <<
                  ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::chmod_(const std::string& path,
                   mode_t mode)
{
    LOG_TRACE("chmod " << path << ", mode " << mode);

    int res = maybeInjectError_(path, FileSystemCall::Chmod);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);

    res = ::chmod(p.string().c_str(), mode);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::chown_(const std::string& path,
                   uid_t uid,
                   gid_t gid)
{
    LOG_TRACE("chown " << path << "uid " << uid << ", gid " << gid);

    int res = maybeInjectError_(path, FileSystemCall::Chown);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);

    res = ::lchown(p.string().c_str(), uid, gid);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::truncate_(const std::string& path,
                      off_t size)
{
    LOG_TRACE("truncate " << path << ", size " << size);

    int res = maybeInjectError_(path, FileSystemCall::Truncate);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);
    res = ::truncate(p.string().c_str(), size);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::ftruncate_(const std::string& path,
                       off_t size,
                       fuse_file_info* /* fi */)
{
    LOG_TRACE("ftruncate " << path << ", size " << size);
    return truncate_(path, size);
}

int
FileSystem::utimens_(const std::string& path,
                     const struct timespec ts[2])
{
    LOG_TRACE("utimens " << path);

    int res = maybeInjectError_(path, FileSystemCall::Utimens);
    if (res != 0)
    {
        return res;
    }

    struct timeval tv[2];

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    const fs::path p(backend_dir_ / path);

    res = ::utimes(p.string().c_str(), tv);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    return 0;
}

int
FileSystem::open_(const std::string& path,
                  struct fuse_file_info *fi)
{
    LOG_TRACE("open " << path << ", fi->flags " << fi->flags);

    int res = maybeInjectError_(path, FileSystemCall::Open);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);

    res = ::open(p.string().c_str(), fi->flags);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    ::close(res);

    fi->direct_io = useDirectIO_(path);
    LOG_TRACE(path << ": fuse direct I/O: " << static_cast<unsigned int>(fi->direct_io));

    return 0;
}

int
FileSystem::read_(const std::string& path,
                  char *buf,
                  size_t size,
                  off_t offset,
                  struct fuse_file_info* /* fi */)
{
    LOG_TRACE("read " << path << ", size " << size << ", off " << offset);

    int res = maybeInjectError_(path, FileSystemCall::Read);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);

    int fd = ::open(p.string().c_str(), O_RDONLY);
    if (fd == -1)
    {
        res = errno;
        LOG_ERROR(p << ": (open) encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    res = ::pread(fd, buf, size, offset);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": (pread) encountered error on underlying filesystem: " <<
                  strerror(res));
        res = -res;
    }

    ::close(fd);
    return res;
}

int
FileSystem::write_(const std::string& path,
                   const char* buf,
                   size_t size,
                   off_t offset,
                   struct fuse_file_info* /* fi */)
{
    LOG_TRACE("write " << path << ", size " << size << ", off " << offset);

    int res = maybeInjectError_(path, FileSystemCall::Write);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);

    int fd = ::open(p.string().c_str(), O_WRONLY);
    if (fd == -1)
    {
        res = errno;
        LOG_ERROR(p << ": (open) encountered error on underlying filesystem: " <<
                  strerror(res));
        return -res;
    }

    res = ::pwrite(fd, buf, size, offset);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": (pread) encountered error on underlying filesystem: " <<
                  strerror(res));
        res = -res;
    }

    ::close(fd);
    return res;
}

int
FileSystem::statfs_(const std::string& path,
                    struct statvfs *stbuf)
{
    LOG_TRACE("statfs " << path);

    int res = maybeInjectError_(path, FileSystemCall::Statfs);
    if (res != 0)
    {
        return res;
    }

    const fs::path p(backend_dir_ / path);

    res = ::statvfs(p.string().c_str(), stbuf);
    if (res == -1)
    {
        res = errno;
        LOG_ERROR(p << ": encountered error on underlying filesystem: " <<
                  strerror(res));
        res = -res;
    }

    return res;
}

int
FileSystem::release_(const std::string& path,
                     struct fuse_file_info* /* fi */)
{
    //    LOG_TRACE("release " << path);

    int res = maybeInjectError_(path, FileSystemCall::Release);
    if (res != 0)
    {
        return res;
    }

    /* Just a stub.	 This method is optional and can safely be left
       unimplemented */
    return 0;
}

int
FileSystem::fsync_(const std::string& path,
                   int isdatasync,
                   struct fuse_file_info* /* fi */)
{
    LOG_TRACE("fsync " << path << ", isdatasync " << isdatasync);

    int res = maybeInjectError_(path, FileSystemCall::Fsync);
    if (res != 0)
    {
        return res;
    }

    /* Just a stub.	 This method is optional and can safely be left
       unimplemented */

    return 0;
}

void
FileSystem::addDelayRule(DelayRulePtr rule)
{
    LOCK();

    delay_rules_t::iterator it = delay_rules_.find(rule->id);
    if (it != delay_rules_.end())
    {
        std::stringstream ss;
        ss << "Delay rule with id " << rule->id << " already present";
        LOG_ERROR(ss.str());
        throw ConfigException(ss.str().c_str());
    }

    std::pair<delay_rules_t::iterator, bool> res =
        delay_rules_.insert(std::make_pair(rule->id,
                                           rule));
    VERIFY(res.second == true);
    LOG_INFO("Added delay rule with id " << rule->id);
}

void
FileSystem::addFailureRule(FailureRulePtr rule)
{
    LOCK();

    failure_rules_t::iterator it = failure_rules_.find(rule->id);
    if (it != failure_rules_.end())
    {
        std::stringstream ss;
        ss << "Failure rule with id " << rule->id << " already present";
        LOG_ERROR(ss.str());
        throw ConfigException(ss.str().c_str());
    }

    std::pair<failure_rules_t::iterator, bool> res =
        failure_rules_.insert(std::make_pair(rule->id,
                                             rule));
    VERIFY(res.second == true);
    LOG_INFO("Added failure rule with id " << rule->id);
}

void
FileSystem::addDirectIORule(DirectIORulePtr rule)
{
    LOCK();

    direct_io_rules_t::iterator it = direct_io_rules_.find(rule->id);
    if (it != direct_io_rules_.end())
    {
        std::stringstream ss;
        ss << "DirectIO rule with id " << rule->id << " already present";
        LOG_ERROR(ss.str());
        throw ConfigException(ss.str().c_str());
    }

    std::pair<direct_io_rules_t::iterator, bool> res =
        direct_io_rules_.insert(std::make_pair(rule->id,
                                               rule));
    VERIFY(res.second == true);
    LOG_INFO("Added DirectIO rule with id " << rule->id);
}

void
FileSystem::removeDelayRule(rule_id id)
{
    LOCK();
    delay_rules_t::iterator it = delay_rules_.find(id);
    if (it == delay_rules_.end())
    {
        std::stringstream ss;
        ss << "Delay rule with id " << id << " does not exist";
        LOG_ERROR(ss.str());
        throw ConfigException(ss.str().c_str());
    }
    else
    {
        DelayRulePtr drp = it->second;
        delay_rules_.erase(it);
        drp->abort();
        LOG_INFO("Removed delay rule with id " << id);
    }
}

void
FileSystem::removeFailureRule(rule_id id)
{
    LOCK();
    size_t res = failure_rules_.erase(id);
    if (res == 0)
    {
        std::stringstream ss;
        ss << "Failure rule with id " << id << " does not exist";
        LOG_ERROR(ss.str());
        throw ConfigException(ss.str().c_str());
    }
    else
    {
        LOG_INFO("Removed failure rule with id " << id);
    }
}

void
FileSystem::removeDirectIORule(rule_id id)
{
    LOCK();
    size_t res = direct_io_rules_.erase(id);
    if (res == 0)
    {
        std::stringstream ss;
        ss << "DirectIO rule with id " << id << " does not exist";
        LOG_ERROR(ss.str());
        throw ConfigException(ss.str().c_str());
    }
    else
    {
        LOG_INFO("Removed DirectIO rule with id " << id);
    }
}

void
FileSystem::listDelayRules(delay_rules_list_t& l)
{
    LOCK();
    BOOST_FOREACH(delay_rules_t::value_type& v, delay_rules_)
    {
        l.push_back(v.second);
    }
}

void
FileSystem::listFailureRules(failure_rules_list_t& l)
{
    LOCK();
    BOOST_FOREACH(failure_rules_t::value_type& v, failure_rules_)
    {
        l.push_back(v.second);
    }
}

void
FileSystem::listDirectIORules(direct_io_rules_list_t& l)
{
    LOCK();
    BOOST_FOREACH(direct_io_rules_t::value_type& v, direct_io_rules_)
    {
        l.push_back(v.second);
    }
}

void
FileSystem::maybeInjectDelay_(const std::string& path,
                              const FileSystemCall call)
{
    DelayRulePtr drp;
    {
        LOCK();
        for(delay_rules_t::value_type& v : delay_rules_)
        {
            bl::tribool res = v.second->match(path, call);
            if (res == true)
            {
                drp = v.second;
                break;
            }
        }
    }

    if (drp != 0)
    {
        LOG_INFO(path << ": injecting delay of " << drp->delay_usecs <<
                 " us to " << call);
        (*drp)();
    }
}

int
FileSystem::maybeInjectFailure_(const std::string& path,
                                const FileSystemCall call)
{
    LOCK();
    for(failure_rules_t::value_type& v : failure_rules_)
    {
        FailureRulePtr r = v.second;
        bl::tribool res = r->match(path, call);
        if (res == true)
        {
            LOG_INFO(path << ": injecting error code " << r->errcode <<
                     " into " << call);
            return -r->errcode;
        }
    }

    return 0;
}

int
FileSystem::maybeInjectError_(const std::string& path,
                              const FileSystemCall call)
{
    maybeInjectDelay_(path, call);
    return maybeInjectFailure_(path, call);
}

bool
FileSystem::useDirectIO_(const std::string& path) const
{
    LOCK();
    for(const direct_io_rules_t::value_type& v : direct_io_rules_)
    {
        const DirectIORulePtr r(v.second);
        if (r->match(path))
        {
            return r->direct_io;
        }
    }

    return false;
}

}

// Local Variables: **
// mode: c++ **
// End: **
