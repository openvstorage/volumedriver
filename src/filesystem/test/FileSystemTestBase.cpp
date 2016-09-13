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

#include "FileSystemTestBase.h"

#include <type_traits>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/system/error_code.hpp>

#include <youtils/Assert.h>
#include <youtils/DimensionedValue.h>
#include <youtils/LockedArakoon.h>
#include <youtils/System.h>

#include <volumedriver/Api.h>
#include <volumedriver/VolumeDriverParameters.h>

#include "../FuseInterface.h"
#include "../ObjectRegistry.h"
#include "../Registry.h"
#include "../VirtualDiskFormat.h"
#include "../VirtualDiskFormatVmdk.h"
#include "../VirtualDiskFormatRaw.h"

namespace volumedriverfstest
{

namespace ara = arakoon;
namespace ba = boost::asio;
namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace vd = volumedriver;
namespace vfs = volumedriverfs;
namespace yt = youtils;
namespace ytt = youtilstest;

fs::path FileSystemTestBase::binary_path_;

#define LOCKVD() \
    fungi::ScopedLock ag__(api::getManagementMutex())

FileSystemTestBase::FileSystemTestBase(const FileSystemTestSetupParameters& params)
    : FileSystemTestSetup(params)
    , remote_pid_(-1)
{}

void
FileSystemTestBase::SetUp()
{
    FileSystemTestSetup::SetUp();
    start_fs(PutClusterNodeConfigsInRegistry::T);
}

void
FileSystemTestBase::start_fs(PutClusterNodeConfigsInRegistry put_configs)
{
    bpt::ptree pt;
    start_fs(pt, put_configs);
}

void
FileSystemTestBase::start_fs(bpt::ptree& pt, PutClusterNodeConfigsInRegistry put_configs)
{
    make_config_(pt,
                 topdir_,
                 local_config().vrouter_id);

    bpt::json_parser::write_json(configuration_.string(), pt);

    if (put_configs == PutClusterNodeConfigsInRegistry::T)
    {
        put_cluster_node_configs_in_registry(pt);
    }

    fs_.reset(new vfs::FileSystem(pt));
}

void
FileSystemTestBase::stop_fs()
{
    fs_.reset();
}

void
FileSystemTestBase::TearDown()
{
    stop_fs();
    FileSystemTestSetup::TearDown();
}

vfs::ObjectId
FileSystemTestBase::create_file(const vfs::FrontendPath& fname)
{
    EXPECT_EQ(0, mknod(fname,
                       S_IFREG bitor S_IWUSR bitor S_IRUSR,
                       0));

    vfs::DirectoryEntryPtr dentry(fs_->mdstore_.find_throw(fname));
    VERIFY(dentry != nullptr);
    VERIFY(dentry->type() != vfs::DirectoryEntry::Type::Directory);
    return dentry->object_id();
}

vfs::ObjectId
FileSystemTestBase::create_file(const vfs::ObjectId& parent_id,
                                const std::string& name)
{
    EXPECT_EQ(0, mknod(parent_id,
                       name,
                       S_IFREG bitor S_IWUSR bitor S_IRUSR));

    const vfs::FrontendPath fpath(fs_->mdstore_.find_path(parent_id) / name);
    vfs::DirectoryEntryPtr dentry(fs_->mdstore_.find_throw(fpath));
    VERIFY(dentry != nullptr);
    VERIFY(dentry->type() != vfs::DirectoryEntry::Type::Directory);
    return dentry->object_id();
}

vfs::ObjectId
FileSystemTestBase::create_file(const vfs::FrontendPath& path,
                                 uint64_t size)
{
    const vfs::ObjectId id(create_file(path));
    truncate(path, size);
    return id;
}

vfs::ObjectId
FileSystemTestBase::create_file(const vfs::ObjectId& parent_id,
                                const std::string& name,
                                uint64_t size)
{
    const vfs::ObjectId id(create_file(parent_id,
                                       name));
    truncate(id, size);
    return id;
}

void
FileSystemTestBase::create_directory(const vfs::FrontendPath& path)
{
    VERIFY(not fs_->is_volume_path_(path));
    EXPECT_EQ(0, mkdir(path,
                       S_IFDIR bitor S_IRWXU));
}

void
FileSystemTestBase::create_directory(const vfs::ObjectId& parent_id,
                                     const std::string& name)
{
    const vfs::FrontendPath fpath(fs_->mdstore_.find_path(parent_id) / name);

    VERIFY(not fs_->is_volume_path_(fpath));
    EXPECT_EQ(0, mkdir(parent_id,
                       name,
                       S_IFDIR bitor S_IRWXU));
}

void
FileSystemTestBase::destroy_volume(const vfs::FrontendPath& fname)
{
    VERIFY(fs_->is_volume_path_(fname));

    ASSERT_EQ(0, unlink(fname));
    ASSERT_THROW(fs_->mdstore_.find_throw(fname),
                 vfs::HierarchicalArakoon::DoesNotExistException);
}

void
FileSystemTestBase::destroy_volume(const vfs::ObjectId& parent_id,
                                   const std::string& name)
{
    const vfs::FrontendPath fpath(fs_->mdstore_.find_path(parent_id) / name);
    VERIFY(fs_->is_volume_path_(fpath));

    ASSERT_EQ(0, unlink(parent_id, name));
    ASSERT_THROW(fs_->mdstore_.find_throw(fpath),
                 vfs::HierarchicalArakoon::DoesNotExistException);
}

ssize_t
FileSystemTestBase::write_to_file(const vfs::FrontendPath& fname,
                                  const char* buf,
                                  uint64_t size,
                                  off_t off)
{
    vfs::Handle::Ptr h;

    EXPECT_EQ(0, open(fname, h, O_WRONLY));

    FileSystemTestBase* fst = this;

    BOOST_SCOPE_EXIT((fst)(&fname)(&h))
    {
        EXPECT_EQ(0, fst->release(fname, std::move(h)));
    }
    BOOST_SCOPE_EXIT_END;

    return write(fname, &buf[0], size, off, *h);
}

ssize_t
FileSystemTestBase::write_to_file(const vfs::ObjectId& id,
                                  const char* buf,
                                  uint64_t size,
                                  off_t off)
{
    vfs::Handle::Ptr h;

    EXPECT_EQ(0, open(id, h, O_WRONLY));

    FileSystemTestBase* fst = this;

    BOOST_SCOPE_EXIT((fst)(&h))
    {
        EXPECT_NO_THROW(fst->release(std::move(h)));
    }
    BOOST_SCOPE_EXIT_END;

    return write(*h, &buf[0], size, off);
}

ssize_t
FileSystemTestBase::write_to_file(const vfs::FrontendPath& fname,
                                   const std::string& pattern,
                                   uint64_t size,
                                   off_t off)
{
    EXPECT_FALSE(pattern.empty()) << "fix your test";
    VERIFY(pattern.size() != 0); // hey clang analyzer, this one's for you.

    std::vector<char> buf(size);
    for (uint64_t i = 0; i < size; ++i)
    {
        buf[i] = pattern[i % pattern.size()];
    }

    return write_to_file(fname, &buf[0], buf.size(), off);
}

ssize_t
FileSystemTestBase::write_to_file(const vfs::ObjectId& id,
                                  const std::string& pattern,
                                  uint64_t size,
                                  off_t off)
{
    EXPECT_FALSE(pattern.empty()) << "fix your test";
    VERIFY(pattern.size() != 0); // hey clang analyzer, this one's for you.

    std::vector<char> buf(size);
    for (uint64_t i = 0; i < size; ++i)
    {
        buf[i] = pattern[i % pattern.size()];
    }

    return write_to_file(id, &buf[0], buf.size(), off);
}

ssize_t
FileSystemTestBase::read_from_file(const vfs::FrontendPath& fname,
                                    char* buf,
                                    uint64_t size,
                                    off_t off)
{
    vfs::Handle::Ptr h;

    // ASSERT_EQ just does not work here so we'll have to have this crude construct.
    int ret = open(fname, h, O_RDONLY);
    EXPECT_EQ(0, ret);
    if (ret != 0)
    {
        return ret;
    }
    else
    {
        FileSystemTestBase* fst = this;

        BOOST_SCOPE_EXIT((fst)(&fname)(&h))
        {
            EXPECT_EQ(0, fst->release(fname, std::move(h)));
        }
        BOOST_SCOPE_EXIT_END;

        return read(fname, buf, size, off, *h);
    }
}

ssize_t
FileSystemTestBase::read_from_file(const vfs::ObjectId& id,
                                   char *buf,
                                   uint64_t size,
                                   off_t off)
{
    vfs::Handle::Ptr h;

    int ret = open(id, h, O_RDONLY);
    EXPECT_EQ(0, ret);
    if (ret != 0)
    {
        return ret;
    }
    else
    {
        FileSystemTestBase* fst = this;

        BOOST_SCOPE_EXIT((fst)(&h))
        {
            EXPECT_EQ(0, fst->release(std::move(h)));
        }
        BOOST_SCOPE_EXIT_END;

        return read(*h, buf, size, off);
    }
}

int
FileSystemTestBase::opendir(const vfs::FrontendPath& path,
                            vfs::Handle::Ptr& h)
{
    return vfs::FuseInterface::convert_exceptions<decltype(h)>(&vfs::FileSystem::opendir,
                                                               *fs_,
                                                               path,
                                                               h);
}

int
FileSystemTestBase::releasedir(const vfs::FrontendPath& path,
                               vfs::Handle::Ptr h)
{
    return vfs::FuseInterface::convert_exceptions<decltype(h)>(&vfs::FileSystem::releasedir,
                                                               *fs_,
                                                               path,
                                                               std::move(h));
}

int
FileSystemTestBase::mkdir(const vfs::FrontendPath& path,
                          mode_t mode)
{
    const vfs::UserId uid(::getuid());
    const vfs::GroupId gid(::getgid());
    const vfs::Permissions pms(mode);

    return vfs::FuseInterface::convert_exceptions<vfs::UserId,
                                                  vfs::GroupId,
                                                  vfs::Permissions>(&vfs::FileSystem::mkdir,
                                                                    *fs_,
                                                                    path,
                                                                    uid,
                                                                    gid,
                                                                    pms);
}

int
FileSystemTestBase::mkdir(const vfs::ObjectId& parent_id,
                          const std::string& name,
                          mode_t mode)
{
    const vfs::UserId uid(::getuid());
    const vfs::GroupId gid(::getgid());
    const vfs::Permissions pms(mode);

    return fs_convert_exceptions<const vfs::ObjectId&,
                                 const std::string&,
                                 vfs::UserId,
                                 vfs::GroupId,
                                 vfs::Permissions>(*fs_,
                                                   &vfs::FileSystem::mkdir,
                                                   parent_id,
                                                   name,
                                                   uid,
                                                   gid,
                                                   pms);
}

int
FileSystemTestBase::rmdir(const vfs::FrontendPath& path)
{
    return vfs::FuseInterface::convert_exceptions(&vfs::FileSystem::rmdir,
                                                  *fs_,
                                                  path);
}

int
FileSystemTestBase::rmdir(const vfs::ObjectId& id)
{
    return fs_convert_exceptions<const vfs::ObjectId&>(*fs_,
                                                       &vfs::FileSystem::rmdir,
                                                       id);
}

int
FileSystemTestBase::mknod(const vfs::FrontendPath& path,
                          mode_t mode,
                          dev_t /* rdev */)
{
    const vfs::UserId uid(::getuid());
    const vfs::GroupId gid(::getgid());
    const vfs::Permissions pms(mode);

    return vfs::FuseInterface::convert_exceptions<vfs::UserId,
                                                  vfs::GroupId,
                                                  vfs::Permissions>(&vfs::FileSystem::mknod,
                                                                    *fs_,
                                                                    path,
                                                                    uid,
                                                                    gid,
                                                                    pms);
}

int
FileSystemTestBase::mknod(const vfs::ObjectId& parent_id,
                          const std::string& name,
                          mode_t mode)
{
    const vfs::UserId uid(::getuid());
    const vfs::GroupId gid(::getgid());
    const vfs::Permissions pms(mode);

    return fs_convert_exceptions<const vfs::ObjectId&,
                                 const std::string&,
                                 vfs::UserId,
                                 vfs::GroupId,
                                 vfs::Permissions>(*fs_,
                                                   &vfs::FileSystem::mknod,
                                                   parent_id,
                                                   name,
                                                   uid,
                                                   gid,
                                                   pms);
}

int
FileSystemTestBase::unlink(const vfs::FrontendPath& path)
{
    return vfs::FuseInterface::convert_exceptions(&vfs::FileSystem::unlink,
                                                  *fs_,
                                                  path);
}

int
FileSystemTestBase::unlink(const vfs::ObjectId& parent_id,
                           const std::string& name)
{
    return fs_convert_exceptions<const vfs::ObjectId&,
                                 const std::string&>(*fs_,
                                                     &vfs::FileSystem::unlink,
                                                     parent_id,
                                                     name);
}

int
FileSystemTestBase::truncate(const vfs::FrontendPath& path,
                              off_t size)
{
    return vfs::FuseInterface::convert_exceptions(&vfs::FileSystem::truncate,
                                                  *fs_,
                                                  path,
                                                  size);
}

int
FileSystemTestBase::truncate(const vfs::ObjectId& id,
                             off_t size)
{
    return fs_convert_exceptions<const vfs::ObjectId&,
                                 decltype(size)>(*fs_,
                                                 &vfs::FileSystem::truncate,
                                                 id,
                                                 size);
}

int
FileSystemTestBase::getattr(const vfs::FrontendPath& path,
                             struct stat& st)
{
    return vfs::FuseInterface::convert_exceptions<decltype(st)>(&vfs::FileSystem::getattr,
                                                                *fs_,
                                                                path,
                                                                st);
}

int
FileSystemTestBase::getattr(const vfs::ObjectId& id,
                            struct stat& st)
{
    return fs_convert_exceptions<const vfs::ObjectId&,
                                 struct stat&>(*fs_,
                                               &vfs::FileSystem::getattr,
                                               id,
                                               st);
}

int
FileSystemTestBase::rename(const vfs::FrontendPath& from,
                           const vfs::FrontendPath& to,
                           vfs::FileSystem::RenameFlags flags)
{
    return vfs::FuseInterface::convert_exceptions<decltype(to),
                                                  decltype(flags)>(&vfs::FileSystem::rename,
                                                                   *fs_,
                                                                   from,
                                                                   to,
                                                                   flags);
}

int
FileSystemTestBase::rename(const vfs::ObjectId& from_parent_id,
                           const std::string& from_name,
                           const vfs::ObjectId& to_parent_id,
                           const std::string& to_name,
                           vfs::FileSystem::RenameFlags flags)
{
    return fs_convert_exceptions<const vfs::ObjectId&,
                                 const std::string&,
                                 const vfs::ObjectId&,
                                 const std::string&,
                                 decltype(flags)>(*fs_,
                                                  &vfs::FileSystem::rename,
                                                  from_parent_id,
                                                  from_name,
                                                  to_parent_id,
                                                  to_name,
                                                  flags);
}

int
FileSystemTestBase::open(const vfs::FrontendPath& path,
                         vfs::Handle::Ptr& h,
                         int flags)
{
    return vfs::FuseInterface::convert_exceptions<mode_t,
                                                  decltype(h)>(&vfs::FileSystem::open,
                                                               *fs_,
                                                               path,
                                                               flags,
                                                               h);
}

int
FileSystemTestBase::open(const vfs::ObjectId& id,
                         vfs::Handle::Ptr& h,
                         mode_t flags)
{
    return fs_convert_exceptions<const vfs::ObjectId&,
                                 mode_t,
                                 decltype(h)>(*fs_,
                                         &vfs::FileSystem::open,
                                         id,
                                         flags,
                                         h);
}

int
FileSystemTestBase::release(const vfs::FrontendPath& path,
                            vfs::Handle::Ptr h)
{
    return vfs::FuseInterface::convert_exceptions<decltype(h)>(&vfs::FileSystem::release,
                                                               *fs_,
                                                               path,
                                                               std::move(h));
}

int
FileSystemTestBase::release(vfs::Handle::Ptr h)
{
    return fs_convert_exceptions<decltype(h)>(*fs_,
                                              &vfs::FileSystem::release,
                                              std::move(h));
}

int
FileSystemTestBase::write(const vfs::FrontendPath& path,
                          const char* buf,
                          uint64_t size,
                          off_t off,
                          vfs::Handle& h)
{
    int ret = vfs::FuseInterface::convert_exceptions<vfs::Handle&,
                                                     size_t&,
                                                     decltype(buf),
                                                     decltype(off)>(&vfs::FileSystem::write,
                                                                    *fs_,
                                                                    path,
                                                                    h,
                                                                    size,
                                                                    buf,
                                                                    off);
    return ret ? ret : size;
}

int
FileSystemTestBase::write(vfs::Handle& h,
                          const char* buf,
                          uint64_t size,
                          off_t off)
{
    bool sync = false;
    int ret = fs_convert_exceptions<vfs::Handle&,
                                    size_t&,
                                    decltype(buf),
                                    decltype(off),
                                    bool&>(*fs_,
                                           &vfs::FileSystem::write,
                                           h,
                                           size,
                                           buf,
                                           off,
                                           sync);
    return ret ? ret : size;
}

int
FileSystemTestBase::fsync(volumedriverfs::Handle& h,
                          bool datasync)
{
    return fs_convert_exceptions<vfs::Handle&,
                                 bool>(*fs_,
                                       &vfs::FileSystem::fsync,
                                       h,
                                       datasync);
}

int
FileSystemTestBase::read(const vfs::FrontendPath& path,
                         char* buf,
                         uint64_t size,
                         off_t off,
                         vfs::Handle& h)
{
    int ret = vfs::FuseInterface::convert_exceptions<vfs::Handle&,
                                                     size_t&,
                                                     decltype(buf),
                                                     decltype(off)>(&vfs::FileSystem::read,
                                                                    *fs_,
                                                                    path,
                                                                    h,
                                                                    size,
                                                                    buf,
                                                                    off);

    return ret ? ret : size;
}

int
FileSystemTestBase::read(vfs::Handle& h,
                         char *buf,
                         uint64_t size,
                         off_t off)
{
    bool eof;
    int ret = fs_convert_exceptions<vfs::Handle&,
                                    size_t&,
                                    decltype(buf),
                                    decltype(off),
                                    bool&>(*fs_,
                                           &vfs::FileSystem::read,
                                           h,
                                           size,
                                           buf,
                                           off,
                                           eof);
    return ret ? ret : size;
}

int
FileSystemTestBase::chmod(const vfs::FrontendPath& path,
                           mode_t mode)
{
    return vfs::FuseInterface::convert_exceptions<decltype(mode)>(&vfs::FileSystem::chmod,
                                                                  *fs_,
                                                                  path,
                                                                  mode);
}

int
FileSystemTestBase::chmod(const vfs::ObjectId& id,
                          mode_t mode)
{
    return fs_convert_exceptions<const vfs::ObjectId&,
                                 decltype(mode)>(*fs_,
                                                 &vfs::FileSystem::chmod,
                                                 id,
                                                 mode);
}

int
FileSystemTestBase::chown(const vfs::FrontendPath& path,
                          uid_t uid,
                          gid_t gid)
{
    return vfs::FuseInterface::convert_exceptions<decltype(uid),
                                                  decltype(gid)>(&vfs::FileSystem::chown,
                                                                 *fs_,
                                                                 path,
                                                                 uid,
                                                                 gid);
}

int
FileSystemTestBase::chown(const vfs::ObjectId& id,
                          uid_t uid,
                          gid_t gid)
{
    return fs_convert_exceptions<const vfs::ObjectId&,
                                 decltype(uid),
                                 decltype(gid)>(*fs_,
                                                &vfs::FileSystem::chown,
                                                id,
                                                uid,
                                                gid);
}

int
FileSystemTestBase::utimens(const vfs::FrontendPath& path,
                            const struct timespec ts[2])
{
    return vfs::FuseInterface::convert_exceptions<decltype(ts)>(&vfs::FileSystem::utimens,
                                                                *fs_,
                                                                path,
                                                                ts);
}

int
FileSystemTestBase::utimens(const vfs::ObjectId& id,
                            const struct timespec ts[2])
{
    return fs_convert_exceptions<const vfs::ObjectId&,
                                 decltype(ts)>(*fs_,
                                               &vfs::FileSystem::utimens,
                                               id,
                                               ts);
}

bool
FileSystemTestBase::is_mounted(const fs::path& mntpoint)
{
    fs::ifstream ifs("/proc/mounts");
    std::vector<char> buf(4096);
    while (not ifs.eof())
    {
        ifs.getline(&buf[0], buf.size());
        const std::string l(&buf[0], buf.size());

        size_t n = l.find(vrouter_cluster_id());
        if (n == 0)
        {
            LOG_TRACE("found matching line: " << l.c_str());
            const size_t off = vrouter_cluster_id().size() + 1;
            const std::string r(l.substr(off));
            const size_t len = r.find_first_of(" ");
            const std::string m(r.substr(0, len));
            LOG_TRACE("mountpoint: " << m);

            if (mntpoint.string() == m)
            {
                return true;
            }
        }
    }

    return false;
}

void
FileSystemTestBase::mount_remote()
{
    const fs::path pfx(remote_dir(topdir_));
    make_dirs_(pfx);

    bpt::ptree pt;
    make_config_(pt,
                 pfx,
                 remote_config().vrouter_id);

    const fs::path cfg_file(pfx / "volumedriverfs.json");
    bpt::json_parser::write_json(cfg_file.string(), pt);

    std::vector<char*> args;
    // a guesstimate to avoid too many reallocs
    args.reserve(50);

    BOOST_SCOPE_EXIT((&args))
    {
        for (auto a : args)
        {
            free(a);
        }
    }
    BOOST_SCOPE_EXIT_END;

#define CARG(cstr)                          \
    args.push_back(::strdup(cstr))

#define SARG(str)                           \
    CARG((str).c_str())

#define PARG(path)                          \
    SARG((path).string())

    CARG("remote-volumedriverfs");
    // FUSE args
    CARG("-f"); // ALWAYS run in foreground
    CARG("-o");
    CARG("direct_io");
    CARG("-o");
    CARG("noauto_cache");
    CARG("-o");
    CARG("use_ino"); // leave inode allocation to us
    // CARG("-o");
    // CARG("allow_other");
    CARG("-o");
    CARG("default_permissions");
    // VolumeDriverFS args
    CARG("--config");
    PARG(cfg_file);
    CARG("--mountpoint");
    PARG(mount_dir(pfx));
    CARG("--loglevel");
    CARG("info");
    CARG("--logsink");
    PARG(pfx / "volumedriverfs-remote.log");

#undef CARG
#undef SARG
#undef PARG

    args.push_back(nullptr);

    ASSERT_EQ(-1, remote_pid_);

    LOG_INFO("About to spawn binary " <<  binary_path_);
    {
        std::stringstream result;
        for(auto arg : args)
        {
            result << arg << " ";
        }
        LOG_INFO("With args " << result.str());
    }

    remote_pid_ = ::fork();
    ASSERT_LE(0, remote_pid_);

    if (remote_pid_ == 0)
    {
        // child
        auto ret = ::execv(binary_path_.c_str(),
                           args.data());
        ASSERT_EQ(0, ret);
        exit(0);
    }

    ASSERT_LT(0, remote_pid_);

    wait_for_remote_();
}

bool
FileSystemTestBase::fork_and_exec_umount_(const fs::path& mnt)
{
    const pid_t pid = ::fork();
    EXPECT_LE(0, pid);

    if (pid == 0)
    {
        auto ret = ::execlp("fusermount",
                            "fusermount",
                            "-u",
                            mnt.string().c_str(),
                            nullptr);
        EXPECT_EQ(0, ret);
        ::exit(0);
    }
    else if (pid > 0)
    {
        int status;
        auto child_pid = ::waitpid(pid, &status, 0);
        EXPECT_EQ(pid, child_pid);
        EXPECT_TRUE(WIFEXITED(status));
        return WEXITSTATUS(status) == 0;
    }
    else
    {
        LOG_FATAL("Failed to fork off child process: " << strerror(errno));
        return false;
    }
}

void
FileSystemTestBase::umount_remote()
{
    const fs::path pfx(remote_dir(topdir_));
    const fs::path mnt(mount_dir(pfx));

    if (remote_pid_ > 0)
    {
        uint64_t j = 0;

        while (is_mounted(mnt))
        {
            if (not fork_and_exec_umount_(mnt))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                if(j % 5 == 0)
                {
                    LOG_TRACE("Failed to initiate umount of the remote volumedriverfs mounted at " <<
                              mount_dir(pfx) << " after " << (j / 5) << " seconds");
                }
                ++j;
            }
        }

        int status;
        ASSERT_EQ(remote_pid_, ::waitpid(remote_pid_, &status, 0));
        ASSERT_TRUE(WIFEXITED(status));
        ASSERT_EQ(0, WEXITSTATUS(status));
        remote_pid_ = -1;
    }
    else
    {
        LOG_INFO(mnt << " is not mounted!?");
    }

    ASSERT_FALSE(is_mounted(mnt));
}

void
FileSystemTestBase::wait_for_remote_()
{
    bool mounted = false;
    const uint64_t retry_secs = 60;
    const uint64_t sleep_msecs = 50;
    const fs::path pfx(remote_dir(topdir_));
    const fs::path mnt(mount_dir(pfx));

    for (uint64_t i = 0; i < retry_secs * 1000 / sleep_msecs; ++i)
    {
        LOG_TRACE("checking whether the filesystem is mounted at " << mnt);
        mounted = is_mounted(mnt);
        if (mounted)
        {
            LOG_INFO(mnt << " is mounted");
            break;
        }
        else
        {
            LOG_TRACE(mnt << " not yet mounted - sleeping for a few and trying again");
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));
        }
    }

    if (not mounted)
    {
        FAIL() << "remote fs instance is not mounted at " << mnt <<
            " after " << retry_secs << " seconds - giving up";
    }
}

boost::optional<vfs::ObjectId>
FileSystemTestBase::find_object(const vfs::FrontendPath& p)
{
    boost::optional<vfs::ObjectId> id;

    try
    {
        vfs::DirectoryEntryPtr dentry(fs_->mdstore_.find_throw(p));
        return dentry->object_id();
    }
    catch (vfs::HierarchicalArakoon::DoesNotExistException&)
    {}

    return id;
}

template<typename Param>
void
FileSystemTestBase::set_object_router_param_(const Param& param)
{
    bpt::ptree pt;
    fs_->object_router().persist(pt, ReportDefault::F);
    param.persist(pt);

    vd::UpdateReport urep;
    fs_->object_router().update(pt, urep);
    EXPECT_EQ(1U, urep.update_size()) << "fix yer test";
}

void
FileSystemTestBase::set_volume_write_threshold(uint64_t wthresh)
{
    set_object_router_param_(ip::PARAMETER_TYPE(vrouter_volume_write_threshold)(wthresh));
}

void
FileSystemTestBase::set_volume_read_threshold(uint64_t rthresh)
{
    set_object_router_param_(ip::PARAMETER_TYPE(vrouter_volume_read_threshold)(rthresh));
}

void
FileSystemTestBase::set_file_write_threshold(uint64_t wthresh)
{
    set_object_router_param_(ip::PARAMETER_TYPE(vrouter_file_write_threshold)(wthresh));
}

void
FileSystemTestBase::set_file_read_threshold(uint64_t rthresh)
{
    set_object_router_param_(ip::PARAMETER_TYPE(vrouter_file_read_threshold)(rthresh));
}

void
FileSystemTestBase::set_backend_sync_timeout(const boost::chrono::milliseconds& ms)
{
    set_object_router_param_(ip::PARAMETER_TYPE(vrouter_backend_sync_timeout_ms)(ms.count()));
}

void
FileSystemTestBase::set_lock_reaper_interval(uint64_t seconds)
{
    set_object_router_param_(ip::PARAMETER_TYPE(vrouter_lock_reaper_interval)(seconds));
}

void
FileSystemTestBase::set_use_fencing(bool use_fencing)
{
    use_fencing_ = use_fencing;
    set_object_router_param_(ip::PARAMETER_TYPE(vrouter_use_fencing)(use_fencing));
}

void
FileSystemTestBase::check_file_path_sanity(const vfs::FrontendPath& fp)
{
    vfs::FileSystem::verify_file_path_(fp);
}

void
FileSystemTestBase::wait_for_snapshot(const vfs::ObjectId& volume_id,
                                      const std::string& snapshot_name,
                                      uint32_t max_wait_secs)
{
    TODO("AR: expect a VolumeId instead of an ObjectId?");
    LOCKVD();

    const uint32_t wait_msecs = 10;
    const uint32_t max_wait_msecs = max_wait_secs * 1000;
    uint32_t waited = 0;

    while (not api::isVolumeSyncedUpTo(vd::VolumeId(volume_id.str()),
                                       vd::SnapshotName(snapshot_name)))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_msecs));
        waited += wait_msecs;
        ASSERT_GE(max_wait_msecs, waited) << volume_id.str() <<
            ": snapshot " << snapshot_name << " not in backend after " << max_wait_secs <<
            " seconds";
    }
}

vfs::ObjectRegistrationPtr
FileSystemTestBase::find_registration(const vfs::ObjectId& id)
{
    // create a new instance instead of using the one at
    // fs_->object_router().volume_registry() as the fs_ could be a nullptr
    // (cf. stop_fs())
    vfs::NodeId tmp(yt::UUID().str());
    bpt::ptree pt;

    auto registry(std::make_shared<vfs::Registry>(make_registry_config_(pt)));
    vfs::ObjectRegistry reg(vrouter_cluster_id(),
                            tmp,
                            std::static_pointer_cast<yt::LockedArakoon>(registry));

    return reg.find(id);
}

bool
FileSystemTestBase::is_registered(const vfs::ObjectId& id)
{
    return find_registration(id) != nullptr;
}

void
FileSystemTestBase::verify_registration(const vfs::ObjectId& id,
                                        const vfs::NodeId& node_id)
{
    const vfs::ObjectRegistrationPtr reg(find_registration(id));
    ASSERT_TRUE(reg != nullptr);
    ASSERT_EQ(id, reg->volume_id);
    EXPECT_EQ(node_id, reg->node_id);
}

vfs::FrontendPath
FileSystemTestBase::clone_path_to_volume_path(const vfs::FrontendPath& p)
{
    VERIFY(fs_ != nullptr);
    {
        auto fmt = dynamic_cast<vfs::VirtualDiskFormatVmdk*>(fs_->vdisk_format_.get());
        if ( fmt != nullptr)
        {
            return fmt->make_volume_path_(p);
        }
    }
    {
        auto fmt = dynamic_cast<vfs::VirtualDiskFormatRaw*>(fs_->vdisk_format_.get());
        if ( fmt != nullptr)
        {
            return p;
        }
    }
    throw fungi::IOException("unknown vdisk format");
}

void
FileSystemTestBase::check_snapshots(const vfs::ObjectId& volume_id,
                                     const std::vector<std::string>& expected_snapshots)
{
    TODO("AR: expect a VolumeId instead of an ObjectId?");
    const std::vector<std::string> snap_list(client_.list_snapshots(volume_id.str()));
    ASSERT_EQ(static_cast<ssize_t>(expected_snapshots.size()),
              snap_list.size());

    for (uint32_t i = 0; i < expected_snapshots.size(); i++)
    {
        ASSERT_EQ(expected_snapshots[i],
                  snap_list[i]);
    }
}

std::shared_ptr<vfs::LocalNode>
FileSystemTestBase::local_node(vfs::ObjectRouter& router)
{
    return router.local_node_();
}

std::shared_ptr<vfs::ClusterRegistry>
FileSystemTestBase::cluster_registry(vfs::ObjectRouter& router)
{
    return router.cluster_registry_;
}

size_t
FileSystemTestBase::get_cluster_size(const vfs::ObjectId& oid) const
{
    LOCKVD();
    vd::WeakVolumePtr v = api::getVolumePointer(vd::VolumeId(oid.str()));
    return api::GetClusterSize(v);
}

}

// Local Variables: **
// mode: c++ **
// End: **
