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

#include "FileSystemEvents.h"
#include "FileSystemParameters.h"
#include "FuseInterface.h"
#include "ShmOrbInterface.h"

#include <fuse3/fuse_lowlevel.h>

#include <boost/property_tree/ptree.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>
#include <youtils/ScopeExit.h>
#include <youtils/SignalThread.h>

namespace volumedriverfs
{

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace yt = youtils;

namespace
{

DECLARE_LOGGER("FuseInterfaceHelpers");

void
set_handle(fuse_file_info& fi, Handle::Ptr h)
{
    fi.fh = reinterpret_cast<uint64_t>(h.release());
}

Handle*
get_handle(fuse_file_info& fi)
{
    Handle* h(reinterpret_cast<Handle*>(fi.fh));
    VERIFY(h);
    return h;
}

int
need_worker(unsigned available,
            unsigned total,
            void* priv)
{
    const auto fi = static_cast<FuseInterface*>(priv);
    return available == 0 and
        total < fi->max_workers();
}

int
keep_worker(unsigned /* available */,
            unsigned total,
            void* priv)
{
    const auto fi = static_cast<FuseInterface*>(priv);
    return total <= fi->min_workers();
}

// Copied verbatim from FUSE's helper.c since fuse_{setup,teardown} were unexported
// as of FUSE version 3.
TODO("AR: reconsider what's actually needed from this");
struct fuse*
fuse_setup(int argc,
           char *argv[],
           const struct fuse_operations *op,
           size_t op_size,
           char **mountpoint,
           int *multithreaded,
           void *user_data)
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_chan *ch;
    struct fuse *fuse;
    int foreground;
    int res;

    res = fuse_parse_cmdline(&args, mountpoint, multithreaded, &foreground);
    if (res == -1)
        return NULL;

    ch = fuse_mount(*mountpoint, &args);
    if (!ch) {
        fuse_opt_free_args(&args);
        goto err_free;
    }

    fuse = fuse_new(ch, &args, op, op_size, user_data);
    fuse_opt_free_args(&args);
    if (fuse == NULL)
        goto err_unmount;

    res = fuse_daemonize(foreground);
    if (res == -1)
        goto err_unmount;

    res = fuse_set_signal_handlers(fuse_get_session(fuse));
    if (res == -1)
        goto err_unmount;

    return fuse;

    err_unmount:
    fuse_unmount(*mountpoint, ch);
    if (fuse)
        fuse_destroy(fuse);
    err_free:
    free(*mountpoint);
    return NULL;
}

void
fuse_teardown(struct fuse *fuse,
              char *mountpoint)
{
    struct fuse_session *se = fuse_get_session(fuse);
    struct fuse_chan *ch = fuse_session_chan(se);
    fuse_remove_signal_handlers(se);
    fuse_unmount(mountpoint, ch);
    fuse_destroy(fuse);
    free(mountpoint);
}

}

FuseInterface::FuseInterface(const bpt::ptree& pt,
                             const RegisterComponent registerizle)
    : yt::VolumeDriverComponent(registerizle,
                                pt)
    , fuse_min_workers(pt)
    , fuse_max_workers(pt)
    , fs_(pt,
          registerizle)
    , fuse_(nullptr)
    , shm_orb_server_(fs_.enable_shm_interface() ?
                      std::make_unique<ShmOrbInterface>(pt,
                                                        registerizle,
                                                        fs_) :
                      nullptr)
    , network_server_(fs_.enable_network_interface() ?
                      std::make_unique<NetworkXioInterface>(pt,
                                                            registerizle,
                                                            fs_) :
                      nullptr)
{}

void
FuseInterface::init_ops_(fuse_operations& ops) const
{
    bzero(&ops, sizeof(ops));

#define INSTALL_CB(name)                        \
    ops.name = FuseInterface::name

    INSTALL_CB(getattr);
    INSTALL_CB(readdir);
    INSTALL_CB(mkdir);
    INSTALL_CB(unlink);
    INSTALL_CB(rmdir);
    INSTALL_CB(rename);
    INSTALL_CB(truncate);
    INSTALL_CB(open);
    INSTALL_CB(read);
    INSTALL_CB(write);
    INSTALL_CB(statfs);
    INSTALL_CB(release);
    INSTALL_CB(fsync);
    INSTALL_CB(mknod);
    INSTALL_CB(opendir);
    INSTALL_CB(releasedir);
    INSTALL_CB(utimens);
    INSTALL_CB(chmod);
    INSTALL_CB(chown);

#undef INSTALL_CB
}

void
FuseInterface::run_(fuse* fuse,
                    bool mt)
{
    ASSERT(fuse);

    const int res = mt ?
        fuse_loop_mt_with_worker_control(fuse,
                                         this,
                                         need_worker,
                                         keep_worker) :
        fuse_loop(fuse);
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
FuseInterface::operator()(const fs::path& mntpoint,
                          const std::vector<std::string>& fuse_args)
{
    if (not fs::exists(mntpoint))
    {
        LOG_ERROR("Mountpoint " << mntpoint << " does not exist - bailing out");
        throw fungi::IOException("Base dir does not exist",
                                 mntpoint.string().c_str());
    }

    LOG_INFO(fs_.name() << ": mounting at " << mntpoint);

    if (fuse_)
    {
        LOG_ERROR(fs_.name() << ": already mounted?");
        throw ManagementException("already mounted");
    }

    std::vector<char*> fuse_argv(fuse_args.size() + 2);

    auto argv_exit(yt::make_scope_exit([&fuse_argv]
                                       {
                                           for (char* str : fuse_argv)
                                           {
                                               free(str);
                                           }
                                       }));

    fuse_argv[0] = strdup(fs_.name().c_str());
    for (size_t i = 0; i < fuse_args.size(); ++i)
    {
        LOG_INFO((i + 1) << ": " << fuse_args[i]);
        fuse_argv[i + 1] = ::strdup(fuse_args[i].c_str());
    }

    fuse_argv[fuse_args.size() + 1] = ::strdup(mntpoint.string().c_str());

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
    fuse* fuse = fuse_setup(fuse_argv.size(),
                            &fuse_argv[0],
                            &ops,
                            sizeof(ops),
                            &mountpoint,
                            &multithreaded,
                            this);
    if (not fuse)
    {
        LOG_ERROR(fs_.name() << ": fuse_setup_common failed");
        throw Exception("problem running filesystem");
    }

    auto fuse_exit(yt::make_scope_exit([&fuse,
                                        &mountpoint]
                                       {
                                          LOG_INFO("tearing down fuse");
                                           fuse_teardown(fuse,
                                                         mountpoint);
                                       }));

    // FUSE installs handlers for these, but we don't want to be interrupted at all.
    const yt::SignalSet sigset{SIGHUP,
            // SIGINT, // screws up gdb!
            SIGTERM };

    yt::SignalThread sigthread(sigset,
                               [&](int signal)
                               {
                                   LOG_INFO("caught signal " << signal);
                                   // fuse_exit(fuse) does not work here: FUSE's
                                   // read on /dev/fuse was not (and cannot) be
                                   // interrupted by the / a signal and hence
                                   // only notice the exit notification once the read
                                   // returns next time it gets a request from the kernel.
                                   // So for now we just log it until we made sure that
                                   // our code can deal with being interrupted.
                               });

    boost::optional<boost::thread> shm_thread;
    boost::optional<boost::thread> network_thread;

    if (shm_orb_server_)
    {
        std::promise<void> promise;
        std::future<void> future(promise.get_future());

        shm_thread =
            boost::thread([&]
                          {
                              try
                              {
                                  VERIFY(shm_orb_server_);
                                  shm_orb_server_->run(std::move(promise));
                              }
                              CATCH_STD_ALL_LOG_IGNORE("exception running SHM server");
                          });

        future.get();
    }

    auto shm_thread_exit(yt::make_scope_exit([&]
                        {
                            if (shm_thread)
                            {
                                VERIFY(shm_orb_server_);
                                LOG_INFO("stopping SHM server");
                                try
                                {
                                    shm_orb_server_->stop_all_and_exit();
                                }
                                CATCH_STD_ALL_LOG_IGNORE("failed to stop SHM server");
                                LOG_INFO("waiting for shm thread to finish");
                                shm_thread->join();
                            }
                        }));

    if (network_server_)
    {
        std::promise<void> promise;
        std::future<void> future(promise.get_future());

        network_thread = boost::thread([&]
                    {
                          try
                          {
                            network_server_->run(std::move(promise));
                          }
                          CATCH_STD_ALL_LOG_IGNORE("exception running network (xio) server");
                      });

        future.get();
    }

    auto network_thread_exit(yt::make_scope_exit([&]
                {
                    if (network_thread)
                    {
                        VERIFY(network_server_);
                        LOG_INFO("stopping network (xio) server");
                        try
                        {
                            network_server_->shutdown();
                        }
                        CATCH_STD_ALL_LOG_IGNORE("failed to stop network server");
                        LOG_INFO("waiting for network (xio) thread to finish");
                        network_thread->join();
                    }
                }));


    boost::thread fs_thread([&]
                           {
                               try
                               {
                                   run_(fuse,
                                        multithreaded ? true : false);
                               }
                               CATCH_STD_ALL_LOG_IGNORE("exception running FUSE");
                           });

    auto fs_thread_exit(yt::make_scope_exit([&]
                                           {
                                               LOG_INFO("waiting for fs thread to finish");
                                               fs_thread.join();
                                           }));

    TODO("AR: push down to FileSystem?");
    // It seems having finished fuse_setup is sufficient to have the
    // mountpoint available, no need to poll here.
    const auto ev(FileSystemEvents::up_and_running(mntpoint.string()));
    fs_.object_router().event_publisher()->publish(ev);
}

template<typename... A>
int
FuseInterface::route_to_fs_instance_(void (FileSystem::*mem_fun)(const FrontendPath& path,
                                                                 A... args),
                                     const FrontendPath& p,
                                     A...args) throw ()
{
    fuse_context* ctx = fuse_get_context();
    VERIFY(ctx);

    auto fi = static_cast<FuseInterface*>(ctx->private_data);
    VERIFY(fi);

    return convert_exceptions<A...>(mem_fun,
                                    fi->fs_,
                                    p,
                                    std::forward<A>(args)...);
}

template<typename... A>
int
FuseInterface::route_to_fs_instance_(void (FileSystem::*mem_fun)(const FrontendPath& path,
                                                                 A... args),
                                     const char* p,
                                     A...args) throw ()
{
    return route_to_fs_instance_<A...>(mem_fun,
                                       FrontendPath(p),
                                       std::forward<A>(args)...);
}

int
FuseInterface::getattr(const char* path,
                       struct stat* st)
{
    return route_to_fs_instance_<struct stat&>(&FileSystem::getattr,
                                               path,
                                               *st);
}

int
FuseInterface::opendir(const char* path,
                       fuse_file_info* fi)
{
    fi->fh = 0;
    Handle::Ptr h;
    int ret = route_to_fs_instance_<Handle::Ptr&>(&FileSystem::opendir,
                                                  path,
                                                  h);
    set_handle(*fi,
               std::move(h));

    return ret;
}

int
FuseInterface::releasedir(const char* /* path */,
                          fuse_file_info* fi)
{
    Handle::Ptr h(get_handle(*fi));
    fi->fh = 0;

    VERIFY(h != nullptr);

    return route_to_fs_instance_(&FileSystem::releasedir,
                                 h->path(),
                                 std::move(h));
}

int
FuseInterface::readdir(const char* /* path */,
                       void* buf,
                       fuse_fill_dir_t filler,
                       off_t /* offset */,
                       fuse_file_info* fi,
                       // fuse3/fuse.h says that it's ok to ignore
                       // the sole supported flag - FUSE_READDIR_PLUS -
                       // for now
                       fuse_readdir_flags /* readdir_flags */)
{
    Handle* h = get_handle(*fi);
    VERIFY(h != nullptr);

    LOG_TRACE(h->path());


    std::vector<std::string> l;
    int ret = route_to_fs_instance_<std::vector<std::string>&,
                                    size_t>(&FileSystem::read_dirents,
                                            h->path(),
                                            l,
                                            0);
    if (ret == 0)
    {
        TODO("AR: consider using FUSE_FILL_DIR_PLUS");
        if (filler(buf,
                   ".",
                   0,
                   0,
                   static_cast<fuse_fill_dir_flags>(0)))
        {
            return ret;
        }

        if (filler(buf,
                   "..",
                   0,
                   0,
                   static_cast<fuse_fill_dir_flags>(0)))
        {
            return ret;
        }

        for (const auto& e : l)
        {
            if (filler(buf,
                       e.c_str(),
                       0,
                       0,
                       static_cast<fuse_fill_dir_flags>(0)))
            {
                break;
            }
        }
    }

    return ret;
}

int
FuseInterface::mknod(const char* path,
                     mode_t mode,
                     dev_t rdev)
{
    LOG_TRACE(path << ": dev_t " << rdev << ", mode " << std::oct << mode);

    const fuse_context* ctx = fuse_get_context();
    VERIFY(ctx);

    return route_to_fs_instance_(&FileSystem::mknod,
                                 path,
                                 UserId(ctx->uid),
                                 GroupId(ctx->gid),
                                 Permissions(mode));
}

int
FuseInterface::unlink(const char* path)
{
    return route_to_fs_instance_(&FileSystem::unlink,
                                 path);
}

int
FuseInterface::mkdir(const char* path,
                     mode_t mode)
{
    LOG_TRACE(path << ": mode " << std::oct << mode);

    const fuse_context* ctx = fuse_get_context();
    VERIFY(ctx);

    return route_to_fs_instance_(&FileSystem::mkdir,
                                 path,
                                 UserId(ctx->uid),
                                 GroupId(ctx->gid),
                                 Permissions(mode));
}

int
FuseInterface::rmdir(const char* path)
{
    return route_to_fs_instance_(&FileSystem::rmdir,
                                 path);
}

int
FuseInterface::rename(const char* from,
                      const char* to,
                      unsigned flags)
{
    const FrontendPath t(to);
    return route_to_fs_instance_<const FrontendPath&,
                                 FileSystem::RenameFlags>(&FileSystem::rename,
                                                          from,
                                                          t,
                                                          static_cast<FileSystem::RenameFlags>(flags));
}

int
FuseInterface::truncate(const char* path,
                        off_t size)
{
    return route_to_fs_instance_<off_t>(&FileSystem::truncate,
                                        path,
                                        size);
}

int
FuseInterface::open(const char* path,
                    fuse_file_info* fi)
{
    Handle::Ptr h;
    int ret = route_to_fs_instance_<mode_t,
                                    Handle::Ptr&>(&FileSystem::open,
                                                  path,
                                                  fi->flags,
                                                  h);
    set_handle(*fi,
               std::move(h));

    return ret;
}

int
FuseInterface::release(const char* /* path */,
                       fuse_file_info* fi)
{
    Handle::Ptr h(get_handle(*fi));
    fi->fh = 0;
    VERIFY(h);

    return route_to_fs_instance_(&FileSystem::release,
                                 h->path(),
                                 std::move(h));
}

int
FuseInterface::read(const char* /* path */,
                    char* buf,
                    size_t size,
                    off_t off,
                    fuse_file_info* fi)
{
    Handle* h = get_handle(*fi);
    VERIFY(h);

    int ret = route_to_fs_instance_<Handle&,
                                    size_t&,
                                    decltype(buf),
                                    decltype(off)>(&FileSystem::read,
                                                   h->path(),
                                                   *h,
                                                   size,
                                                   buf,
                                                   off);
    return ret ? ret : size;
}

int
FuseInterface::write(const char* /* path */,
                     const char* buf,
                     size_t size,
                     off_t off,
                     fuse_file_info* fi)
{
    Handle* h = get_handle(*fi);
    VERIFY(h);

    int ret = route_to_fs_instance_<Handle&,
                                    size_t&,
                                    decltype(buf),
                                    decltype(off)>(&FileSystem::write,
                                                   h->path(),
                                                   *h,
                                                   size,
                                                   buf,
                                                   off);
    return ret ? ret : size;
}

int
FuseInterface::fsync(const char* /* path */,
                     int datasync,
                     fuse_file_info* fi)
{
    Handle* h = get_handle(*fi);
    VERIFY(h);

    return route_to_fs_instance_<Handle&,
                                 bool>(&FileSystem::fsync,
                                       h->path(),
                                       *h,
                                       datasync);
}

int
FuseInterface::statfs(const char* path,
                      struct statvfs* stbuf)
{
    return route_to_fs_instance_<struct statvfs&>(&FileSystem::statfs,
                                                  path,
                                                  *stbuf);
}

int
FuseInterface::utimens(const char* path,
                       const struct timespec ts[2])
{
    return route_to_fs_instance_(&FileSystem::utimens,
                                 path,
                                 ts);
}

int
FuseInterface::chmod(const char* path,
                     mode_t mode)
{
    return route_to_fs_instance_(&FileSystem::chmod,
                                 path,
                                 mode);
}

int
FuseInterface::chown(const char* path,
                     uid_t uid,
                     gid_t gid)
{
    return route_to_fs_instance_(&FileSystem::chown,
                                 path,
                                 uid,
                                 gid);
}

const char*
FuseInterface::componentName() const
{
    return ip::fuse_component_name;
}

void
FuseInterface::update(const bpt::ptree& pt,
                      yt::UpdateReport& rep)
{
#define U(var)                                  \
        (var).update(pt, rep)

    U(fuse_min_workers);
    U(fuse_max_workers);
#undef U
}

void
FuseInterface::persist(bpt::ptree& pt,
                       const ReportDefault report_default) const
{
#define P(var)                                  \
        (var).persist(pt, report_default)

    P(fuse_min_workers);
    P(fuse_max_workers);

#undef U
}

namespace
{

template<typename T>
bool
check_workers(const T& t,
              yt::ConfigurationReport crep)
{
    if (t.value() == 0)
    {
        crep.push_front(yt::ConfigurationProblem(t.name(),
                                                 t.section_name(),
                                                 "must be >= 1"));
        return false;
    }
    else
    {
        return true;
    }
}

}

bool
FuseInterface::checkConfig(const bpt::ptree& pt,
                           yt::ConfigurationReport& crep) const
{
    bool res = true;

    const ip::PARAMETER_TYPE(fuse_min_workers) minw(pt);
    if (not check_workers(minw,
                          crep))
    {
        res = false;
    }

    const ip::PARAMETER_TYPE(fuse_max_workers) maxw(pt);
    if (not check_workers(maxw,
                          crep))
    {
        res = false;
    }

    return res;
}

}
