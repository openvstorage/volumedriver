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

#include "FileSystemEvents.h"
#include "FileSystemParameters.h"
#include "FuseInterface.h"
#include "ShmOrbInterface.h"

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
                                CATCH_STD_ALL_LOG_IGNORE("sailed to stop SHM server");
                                LOG_INFO("waiting for shm thread to finish");
                                shm_thread->join();
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
                                     const char* frontend_path,
                                     A...args) throw ()
{
    LOG_TRACE(frontend_path);
    const FrontendPath p(frontend_path);

    fuse_context* ctx = fuse_get_context();
    VERIFY(ctx);

    auto fi = static_cast<FuseInterface*>(ctx->private_data);
    VERIFY(fi);

    return convert_exceptions<A...>(mem_fun,
                                    fi->fs_,
                                    p,
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
FuseInterface::releasedir(const char* path,
                          fuse_file_info* fi)
{
    Handle::Ptr h(get_handle(*fi));
    fi->fh = 0;

    return route_to_fs_instance_(&FileSystem::releasedir,
                                 path,
                                 std::move(h));
}

int
FuseInterface::readdir(const char* path,
                       void* buf,
                       fuse_fill_dir_t filler,
                       off_t /* offset */,
                       fuse_file_info* /* fi */)
{
    LOG_TRACE(path);

    std::vector<std::string> l;
    int ret = route_to_fs_instance_<std::vector<std::string>&,
                                    size_t>(&FileSystem::read_dirents,
                                            path,
                                            l,
                                            0);
    if (ret == 0)
    {
        if (filler(buf, ".", 0, 0))
        {
            return ret;
        }

        if (filler(buf, "..", 0, 0))
        {
            return ret;
        }

        for (const auto& e : l)
        {
            if (filler(buf, e.c_str(), 0, 0))
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
                      const char* to)
{
    const FrontendPath t(to);
    return route_to_fs_instance_<const FrontendPath&>(&FileSystem::rename,
                                                      from,
                                                      t);
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
FuseInterface::release(const char* path,
                       fuse_file_info* fi)
{
    Handle::Ptr h(get_handle(*fi));
    fi->fh = 0;

    return route_to_fs_instance_(&FileSystem::release,
                                 path,
                                 std::move(h));
}

int
FuseInterface::read(const char* path,
                    char* buf,
                    size_t size,
                    off_t off,
                    fuse_file_info* fi)
{
    const Handle* h = get_handle(*fi);
    int ret = route_to_fs_instance_<const Handle&,
                                    size_t&,
                                    decltype(buf),
                                    decltype(off)>(&FileSystem::read,
                                                   path,
                                                   *h,
                                                   size,
                                                   buf,
                                                   off);
    return ret ? ret : size;
}

int
FuseInterface::write(const char* path,
                     const char* buf,
                     size_t size,
                     off_t off,
                     fuse_file_info* fi)
{
    const Handle* h = get_handle(*fi);
    int ret = route_to_fs_instance_<const Handle&,
                                    size_t&,
                                    decltype(buf),
                                    decltype(off)>(&FileSystem::write,
                                                   path,
                                                   *h,
                                                   size,
                                                   buf,
                                                   off);
    return ret ? ret : size;
}

int
FuseInterface::fsync(const char* path,
                     int datasync,
                     fuse_file_info* fi)
{
    const Handle* h = get_handle(*fi);
    return route_to_fs_instance_<const Handle&,
                                 bool>(&FileSystem::fsync,
                                       path,
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
