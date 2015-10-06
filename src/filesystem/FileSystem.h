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

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include "DirectoryEntry.h"
#include "FileEventRule.h"
#include "FileSystemCall.h"
#include "FileSystemParameters.h"
#include "FrontendPath.h"
#include "Handle.h"
#include "HierarchicalArakoon.h"
#include "MetaDataStore.h"
#include "Object.h"
#include "VirtualDiskFormat.h"
#include "ObjectRouter.h"
#include "CloneFileFlags.h"

#include <functional>
#include <system_error>
#include <thread>
#include <vector>

#include <sys/statvfs.h>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/Catchers.h>
#include <youtils/InitializedParam.h>
#include <youtils/IOException.h>
#include <youtils/VolumeDriverComponent.h>

#include <xmlrpc++0.7/src/Server.h>

#include <volumedriver/VolManager.h>

namespace events
{
class Event;
}

namespace volumedriver
{
class MetaDataBackendConfig;
}

namespace volumedriverfstest
{
class DirectoryTest;
class FileSystemTestBase;
class VolumeTest;
}

namespace volumedriverfs
{

#define LOG_N_THROW(code, msg)                                      \
    do                                                              \
    {                                                               \
        int err = code;                                             \
        VERIFY(err);                                                \
        LOG_ERROR(msg << ": " << strerror(err));                    \
        throw std::system_error(err, std::system_category());       \
    }                                                               \
    while (false)

MAKE_EXCEPTION(ManagementException, Exception);
MAKE_EXCEPTION(InternalNameException, Exception);
MAKE_EXCEPTION(FileExistsException, Exception);

// This shall henceforth be known as volumedriverfs' ugly getattr_ hack:
// there are frequent lookups of inexistent entries and we don't want to spam
// the log with error messages about these. So it gets its own exception to
// avoid logging in convert_exceptions_.
// Perhaps an indication that we should not be logging there at all?
MAKE_EXCEPTION(GetAttrOnInexistentPath, Exception);

class EventMessage;
class Registry;
class VolumeRegistry;

class FileSystem
    : public youtils::VolumeDriverComponent
{
    friend class volumedriverfstest::DirectoryTest;
    friend class volumedriverfstest::FileSystemTestBase;
    friend class volumedriverfstest::VolumeTest;

public:
    explicit FileSystem(const boost::property_tree::ptree& pt,
                        const RegisterComponent registerize = RegisterComponent::T);

    ~FileSystem();

    FileSystem(const FileSystem&) = delete;

    FileSystem&
    operator=(const FileSystem&) = delete;

    static void
    destroy(const boost::property_tree::ptree& pt);

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

    void
    read(const Handle& h,
         size_t& size,
         char* buf,
         off_t off,
         bool& eof);

    void
    read(const FrontendPath& path,
         const Handle& h,
         size_t& size,
         char* buf,
         off_t off);

    void
    write(const Handle& h,
          size_t& size,
          const char* buf,
          off_t off,
          bool& sync);

    void
    write(const FrontendPath& path,
          const Handle& h,
          size_t& size,
          const char* buf,
          off_t off);

    void
    fsync(const FrontendPath& path,
          const Handle& h,
          bool datasync);

    void
    fsync(const Handle& h,
          bool datasync);

    template<typename T>
    void
    getattr(const T& entity,
            struct stat& st)
    {
        LOG_TRACE(entity);

        memset(&st, 0x0, sizeof(st));

        // Calling getattr on an inexistent path is not something we want to make a
        // big fuss about as it happens quite frequently. Hence it gets special
        // treatment to not confuse the esteemed readers of our log output with scary
        // error messages.
        DirectoryEntryPtr dentry(mdstore_.find(entity));
        if (dentry == nullptr)
        {
            LOG_TRACE(entity << ": does not exist");
            throw GetAttrOnInexistentPath("Path/UUID does not exist",
                                          entity.str().c_str(),
                                          ENOENT);
        }

        uint64_t size = 0;

        if (dentry->type() == DirectoryEntry::Type::Directory)
        {
            static const unsigned blocksize = 4096;
            st.st_mode = S_IFDIR;
            size = blocksize;
        }
        else
        {
            st.st_mode = S_IFREG;
            size = router_.get_size(dentry->object_id());
        }

        st.st_mode |= dentry->permissions();
        st.st_uid = dentry->user_id();
        st.st_gid = dentry->group_id();
        st.st_ino = dentry->inode();
        st.st_size = size;
        st.st_atime = dentry->atime().tv_sec;
        st.st_ctime = dentry->ctime().tv_sec;
        st.st_mtime = dentry->mtime().tv_sec;
        // We don't support hard links, so files always have a link count of 1.
        // The link count of directories is supposed to be the number of
        // subdirectories + 2 ("." and ".."). However, this
        // http://sourceforge.net/p/fuse/mailman/message/29281571/
        // suggests that a link count of 1 also works so let's use that to avoid
        // expensive metadata lookups as long as we don't run into issues.
        st.st_nlink = 1;
        st.st_blksize = 1 << 9;
        st.st_blocks = (size + ((1 << 9) - 1)) >> 9;
    }

    void
    opendir(const FrontendPath&,
            Handle::Ptr&);

    void
    releasedir(const FrontendPath&,
               Handle::Ptr);

    template<typename T>
    void
    read_dirents(const T& entity,
                 std::vector<std::string>& out,
                 size_t start)
    {
        LOG_TRACE(entity);

        DirectoryEntryPtr dentry(mdstore_.find_throw(entity));

        if (dentry->type() == DirectoryEntry::Type::Directory)
        {
            std::list<std::string> l(mdstore_.list(entity));
            size_t counter = 0;

            for (const auto& e : l)
            {
                if (++counter > start)
                {
                    out.push_back(e);
                }
            }
        }
        else
        {
            LOG_N_THROW(ENOTDIR,
                        entity << " is not a directory");
        }
    }

    void
    vaai_copy(const FrontendPath& src_path,
              const FrontendPath& dst_path,
              const uint64_t& timeout,
              const CloneFileFlags& flags);

    void
    mknod(const FrontendPath& path,
          UserId uid,
          GroupId gid,
          Permissions pms);

    void
    mknod(const ObjectId& parent_id,
          const std::string& name,
          UserId uid,
          GroupId gid,
          Permissions pms);

    void
    mkdir(const FrontendPath& path,
          UserId uid,
          GroupId gid,
          Permissions pms);

    void
    mkdir(const ObjectId& parent_id,
          const std::string& name,
          UserId uid,
          GroupId gid,
          Permissions pms);

    void
    unlink(const FrontendPath& path);

    void
    unlink(const ObjectId& parent_id,
           const std::string& name);

    void
    rmdir(const FrontendPath& path);

    void
    rmdir(const ObjectId& id);

    void
    rename(const FrontendPath& from,
           const FrontendPath& to);

    void
    rename(const ObjectId& from_parent_id,
           const std::string& from,
           const ObjectId& to_parent_id,
           const std::string& to);

    void
    truncate(const FrontendPath& path,
             off_t size);

    void
    truncate(const ObjectId& id,
             off_t size);

    void
    open(const FrontendPath&,
         mode_t openflags,
         Handle::Ptr&);

    void
    open(const ObjectId&,
         mode_t openflags,
         Handle::Ptr&);

    void
    release(const FrontendPath&,
            Handle::Ptr h);

    void
    release(Handle::Ptr h);

    template<typename T>
    void
    statfs(const T& entity,
           struct statvfs& stbuf)
    {
        LOG_TRACE(entity);

        TODO("AR: fill in meaningful values");
        const uint64_t sixty_four_tib = 64ULL << 40;
        const uint64_t bsize = 4096;
        const uint64_t inodes = 1ULL << 20;

        stbuf.f_bsize = bsize;
        stbuf.f_frsize = bsize;
        stbuf.f_blocks = sixty_four_tib / bsize;
        stbuf.f_bfree = sixty_four_tib / bsize;
        stbuf.f_bavail = sixty_four_tib / bsize;
        stbuf.f_files = inodes;
        stbuf.f_ffree = inodes;
        stbuf.f_favail = inodes;
        stbuf.f_fsid = 0xbeefcafe;
        stbuf.f_flag = 0;
        stbuf.f_namemax = 255;
    }

    template<typename T>
    void
    utimens(const T& t,
            const struct timespec ts[2])
    {
        LOG_TRACE(t);

        timeval tv[2];

        if (ts == nullptr)
        {
            int ret = ::gettimeofday(tv,
                                     nullptr);
            if (ret < 0)
            {
                auto ret = ::gettimeofday(tv,
                                          nullptr);
                if (ret < 0)
                {
                    ret = errno;
                    LOG_ERROR("gettimeofday failed: " << strerror(ret));
                    throw fungi::IOException("gettimeofday failed",
                                             "gettimeofday",
                                             ret);
                }
            }

            tv[1] = tv[0];
        }
        else
        {
            tv[0].tv_sec = ts[0].tv_sec;
            tv[0].tv_usec = ts[0].tv_nsec / 1000;
            tv[1].tv_sec = ts[1].tv_sec;
            tv[1].tv_usec = ts[1].tv_nsec / 1000;
        }

        mdstore_.utimes(t,
                        tv[0],
                        tv[1]);
    }

    template<typename T>
    void
    chmod(const T& entity,
           mode_t mode)
    {
        LOG_TRACE(entity << ": mode " << std::oct << mode);
        mdstore_.chmod(entity, Permissions(mode));
    }

    template<typename T>
    void
    chown(const T& entity,
           uid_t uid,
           gid_t gid)
    {
        LOG_TRACE(entity << ": uid " << uid << ", gid " << gid);

        boost::optional<UserId> u(uid);
        if (uid == static_cast<uid_t>(-1))
        {
            u = boost::none;
        }

        boost::optional<GroupId> g(gid);
        if (gid == static_cast<gid_t>(-1))
        {
            g = boost::none;
        }

        if (u or g)
        {
            mdstore_.chown(entity, u, g);
        }
    }

    // NON-FS calls
    volumedriver::VolumeId
    create_clone(const FrontendPath& path_to_clone,
                 volumedriver::VolumeConfig::MetaDataBackendConfigPtr mdb_config,
                 const volumedriver::VolumeId& parent_id,
                 const MaybeSnapshotName& maybe_parent_snap);

    volumedriver::VolumeId
    create_volume(const FrontendPath& path_to_clone,
                  volumedriver::VolumeConfig::MetaDataBackendConfigPtr mdb_config,
                  const uint64_t vsize);

    void
    migrate(const ObjectId& id);

    boost::optional<volumedriver::VolumeId>
    get_volume_id(const FrontendPath& p);

    boost::optional<volumedriver::VolumeId>
    get_volume_id(const ObjectId& id);

    FrontendPath
    find_path(const ObjectId& id);

    boost::optional<ObjectId>
    find_id(const FrontendPath& path);

    // Not nice, but the alternative is to provide a number of methods that call
    // their vrouter equivalents.
    ObjectRouter&
    object_router()
    {
        return router_;
    }

    VirtualDiskFormat&
    vdisk_format()
    {
        return *vdisk_format_;
    }

    inline std::string
    name() const
    {
        return router_.cluster_id();
    }

    void
    drop_from_cache(const FrontendPath&);

    std::unique_ptr<volumedriver::MetaDataBackendConfig>
    make_metadata_backend_config();
private:
    DECLARE_LOGGER("FileSystem");

    const std::vector<FileEventRule> file_event_rules_;

    std::unique_ptr<VirtualDiskFormat> vdisk_format_;

    mutable boost::mutex config_lock_;

    DECLARE_PARAMETER(fs_ignore_sync);
    DECLARE_PARAMETER(fs_internal_suffix);
    DECLARE_PARAMETER(fs_metadata_backend_type);
    DECLARE_PARAMETER(fs_metadata_backend_arakoon_cluster_id);
    DECLARE_PARAMETER(fs_metadata_backend_arakoon_cluster_nodes);
    DECLARE_PARAMETER(fs_metadata_backend_mds_nodes);
    DECLARE_PARAMETER(fs_metadata_backend_mds_apply_relocations_to_slaves);
    DECLARE_PARAMETER(fs_cache_dentries);
    DECLARE_PARAMETER(fs_nullio);
    DECLARE_PARAMETER(fs_dtl_config_mode);
    DECLARE_PARAMETER(fs_dtl_host);
    DECLARE_PARAMETER(fs_dtl_port);

    std::shared_ptr<Registry> registry_;
    ObjectRouter router_;
    MetaDataStore mdstore_;
    xmlrpc::Server xmlrpc_svc_;

    typedef boost::archive::text_iarchive iarchive_type;
    typedef boost::archive::text_oarchive oarchive_type;

    // these are only here to allow testing - otherwise they could perfectly well
    // be hidden in an anon namespace.
    static void
    verify_volume_suffix_(const std::string& sfx);

    static void
    verify_file_path_(const FrontendPath& filepath);

    bool
    is_volume_path_(const FrontendPath& p) const;

    template<typename T>
    void
    update_parent_mtime(const T& entity)
    {
        struct timespec timebuf[2];
        int rc = clock_gettime(CLOCK_REALTIME, &timebuf[0]);
        if (rc != 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        timebuf[1] = timebuf[0];
        utimens(entity,
                timebuf);
    }

    template<typename... A>
    void
    maybe_publish_file_event_(const FileSystemCall call,
                              events::Event (*fun)(const FrontendPath&,
                                                   A... args),
                              const FrontendPath& path,
                              A... args);

    void
    maybe_publish_file_rename_(const FrontendPath& from,
                               const FrontendPath& to);

    void
    restart_();

    void
    create_volume_(const FrontendPath& path,
                   volumedriver::VolumeConfig::MetaDataBackendConfigPtr mdb_config,
                   const uint64_t size,
                   DirectoryEntryPtr dentry);

    void
    create_clone_(const FrontendPath& path,
                  volumedriver::VolumeConfig::MetaDataBackendConfigPtr mdb_config,
                  const volumedriver::VolumeId& parent,
                  const MaybeSnapshotName& maybe_parent_snap,
                  DirectoryEntryPtr dentry);

    typedef std::function<void(const FrontendPath&,
                               DirectoryEntryPtr)> CreateVolumeOrCloneFun;
    volumedriver::VolumeId
    create_volume_or_clone_(const FrontendPath&,
                            CreateVolumeOrCloneFun&&);

    template<typename ...A>
    void
    do_mknod(const FrontendPath& path,
             UserId uid,
             GroupId gid,
             Permissions pms,
             A... args);

    template<typename ...A>
    void
    do_mkdir(UserId uid,
             GroupId gid,
             Permissions pms,
             A... args);

    template<typename ...A>
    void
    do_unlink(const FrontendPath& path,
              const DirectoryEntryPtr& dentry,
              A... args);
};

}

#endif // !FILESYSTEM_H_
