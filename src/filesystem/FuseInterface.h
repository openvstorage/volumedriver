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

#ifndef VFS_FUSE_INTERFACE_H_
#define VFS_FUSE_INTERFACE_H_

#include <system_error>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <youtils/VolumeDriverComponent.h>

#define FUSE_USE_VERSION 30
#include "FileSystem.h"
#include "ShmOrbInterface.h"
#include "NetworkXioInterface.h"

#include <fuse3/fuse.h>

struct fuse_operations;

namespace volumedriverfs
{

class FuseInterface
    : public youtils::VolumeDriverComponent
{
public:
    FuseInterface(const boost::property_tree::ptree& pt,
                  const RegisterComponent registerize = RegisterComponent::T);

    ~FuseInterface() = default;

    FuseInterface(const FuseInterface&) = delete;

    FuseInterface&
    operator=(const FuseInterface&) = delete;

    void
    operator()(const boost::filesystem::path& mntpoint,
               const std::vector<std::string>& fuse_args);

    // The following static calls are hooked into FUSE, i.e.
    // they will not throw but return -errno codes in case of error.
    // Internally these are routed to the filesystem instance and through a
    // exception -> errno code conversion handler (see route_to_fs_instance_() and
    // convert_exceptions() below).
    static int
    getattr(const char* path,
            struct stat* st);

    static int
    opendir(const char* path,
            fuse_file_info* fi);

    static int
    releasedir(const char* path,
               fuse_file_info* fi);

    static int
    readdir(const char* path,
            void* buf,
            fuse_fill_dir_t filler,
            off_t offset,
            fuse_file_info* fi,
            fuse_readdir_flags);

    static int
    mknod(const char* path,
          mode_t mode,
          dev_t rdev);

    static int
    mkdir(const char* path,
          mode_t mode);

    static int
    unlink(const char* path);

    static int
    rmdir(const char* path);

    static int
    rename(const char* from,
           const char* to,
           unsigned flags);

    static int
    truncate(const char* path,
             off_t size);

    static int
    ftruncate(const char* path,
              off_t size,
              fuse_file_info* fi);

    static int
    open(const char* path,
         fuse_file_info* fi);

    static int
    read(const char* path,
         char* buf,
         size_t size,
         off_t off,
         fuse_file_info* fi);

    static int
    write(const char* path,
          const char* buf,
          size_t size,
          off_t off,
          fuse_file_info* fi);

    static int
    statfs(const char* path,
           struct statvfs* stbuf);

    static int
    release(const char* path,
            fuse_file_info* fi);

    static int
    fsync(const char* path,
          int datasync,
          fuse_file_info* fi);

    static int
    utimens(const char* path,
            const struct timespec tv[2]);

    static int
    chmod(const char* path,
          mode_t mode);

    static int
    chown(const char* path,
          uid_t uid,
          gid_t gid);

    virtual const char*
    componentName() const override final;

    virtual void
    update(const boost::property_tree::ptree& pt,
           youtils::UpdateReport& rep) override final;

    virtual void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault reportDefault) const override final;

    virtual bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final;

    // This is used by FileSystemTestBase as well - hence the definition needs
    // to be available. This has the downside of dragging in
    // <volumedriver/VolManager.h>, <boost/system/system_error.hpp>, <system_error>
    // etc.
    template<typename... A>
    static int
    convert_exceptions(void (FileSystem::*mem_fun)(const FrontendPath& path,
                                                   A... args),
                       FileSystem& fs,
                       const FrontendPath& path,
                       A... args) throw ()
    {
        LOG_TRACE(path);

        int ret = 0;

        // consolidate these exceptions
        try
        {
            ((&fs)->*mem_fun)(path,
                              std::forward<A>(args)...);
        }
        catch (GetAttrOnInexistentPath&)
        {
            LOG_TRACE(path << ": getattr on inexistent path");
            ret = -ENOENT;
        }
        catch (InternalNameException& e)
        {
            LOG_ERROR(e.what());
            ret = -EINVAL;
        }
        catch (volumedriver::VolManager::VolumeDoesNotExistException& e)
        {
            LOG_ERROR(path << ": " << e.what());
            ret = -ENOENT;
        }
        catch (ObjectNotRegisteredException& e)
        {
            LOG_ERROR(path << ": " << e.what());
            ret = -ENOENT;
        }
        catch (ConflictingUpdateException& e)
        {
            LOG_ERROR(path << ": " << e.what());
            ret = -EAGAIN;
        }
        catch (std::system_error& e)
        {
            LOG_ERROR(path << ": caught std::system_error " << e.what());
            ret = -e.code().value();
        }
        catch (boost::system::system_error& e)
        {
            LOG_ERROR(path << ": caught boost::system::system_error " << e.what());
            ret = -e.code().value();
        }
        catch (std::error_code& e)
        {
            LOG_ERROR(path << ": caught std::error_code: " << e.message());
            ret = -e.value();
        }
        catch (boost::system::error_code& e)
        {
            LOG_ERROR(path << ": caught boost::system::error_code: " << e.message());
            ret = -e.value();
        }
        catch (fungi::IOException& e)
        {
            LOG_ERROR(path << ": caught fungi::IOException: " << e.what() <<
                      ", code " << e.getErrorCode());
            ret = -e.getErrorCode();
            // There are a lot of call (throw) sites that don't set the error code and
            // the default is 0.
            if (ret >= 0)
            {
                ret = -EIO;
            }
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR(path << ": caught exception " << EWHAT <<
                          ": returning I/O error");
                ret = -EIO;
            });

        if (ret < 0)
        {
            fs.drop_from_cache(path);
        }

        return ret;
    }

    uint32_t
    min_workers() const
    {
        return fuse_min_workers.value();
    }

    uint32_t
    max_workers() const
    {
        return fuse_max_workers.value();
    }

private:
    DECLARE_LOGGER("FuseInterface");

    DECLARE_PARAMETER(fuse_min_workers);
    DECLARE_PARAMETER(fuse_max_workers);

    FileSystem fs_;
    fuse* fuse_;
    std::unique_ptr<ShmOrbInterface> shm_orb_server_;
    std::unique_ptr<NetworkXioInterface> network_server_;

    void
    init_ops_(fuse_operations& ops) const;

    void
    run_(fuse* fuse,
         bool multithreaded);

    boost::thread
    init_sighandler_(fuse* fuse);

    template<typename... A>
    static int
    route_to_fs_instance_(void (FileSystem::*mem_fun)(const FrontendPath& path,
                                                      A... args),
                          const char* frontend_path,
                          A... args) throw ();
};

}

#endif // !VFS_FUSE_INTERFACE_H_
