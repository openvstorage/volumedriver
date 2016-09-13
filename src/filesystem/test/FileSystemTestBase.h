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


#ifndef FILE_SYSTEM_TEST_SETUP_H_
#define FILE_SYSTEM_TEST_SETUP_H_

#include "FileSystemTestSetup.h"

#include "../ClusterNodeConfig.h"
#include "../DirectoryEntry.h"
#include "../FileSystem.h"
#include "../LocalNode.h"
#include "../NodeId.h"
#include "../ObjectRegistration.h"

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include <youtils/ArakoonTestSetup.h>
#include <youtils/Logging.h>
#include <gtest/gtest.h>

#include <backend/BackendTestSetup.h>

namespace volumedriverfstest
{

VD_BOOLEAN_ENUM(PutClusterNodeConfigsInRegistry);

class FileSystemTestBase
    : public FileSystemTestSetup
{
public:
    static void
    set_binary_path(const boost::filesystem::path& p)
    {
        binary_path_ = p;
    }

protected:
    explicit FileSystemTestBase(const FileSystemTestSetupParameters& params);

    virtual void
    SetUp();

    virtual void
    TearDown();

    void
    start_fs(PutClusterNodeConfigsInRegistry put_configs =
             PutClusterNodeConfigsInRegistry::F);

    void
    start_fs(boost::property_tree::ptree& pt,
             PutClusterNodeConfigsInRegistry put_configs =
             PutClusterNodeConfigsInRegistry::F);

    void
    stop_fs();

    void
    create_directory(const volumedriverfs::FrontendPath& path);

    void
    create_directory(const volumedriverfs::ObjectId& parent_id,
                     const std::string& name);

    volumedriverfs::ObjectId
    create_file(const volumedriverfs::FrontendPath& fname);

    volumedriverfs::ObjectId
    create_file(const volumedriverfs::ObjectId& parent_id,
                const std::string& name);

    volumedriverfs::ObjectId
    create_file(const volumedriverfs::FrontendPath& fname,
                  uint64_t size);

    volumedriverfs::ObjectId
    create_file(const volumedriverfs::ObjectId& parent_id,
                const std::string& name,
                uint64_t size);

    void
    destroy_volume(const volumedriverfs::FrontendPath& fname);

    void
    destroy_volume(const volumedriverfs::ObjectId& parent_id,
                   const std::string& name);

    ssize_t
    write_to_file(const volumedriverfs::FrontendPath& fname,
                  const std::string& pattern,
                  uint64_t size,
                  off_t off);

    ssize_t
    write_to_file(const volumedriverfs::ObjectId& id,
                  const std::string& pattern,
                  uint64_t size,
                  off_t off);

    ssize_t
    write_to_file(const volumedriverfs::FrontendPath& fname,
                  const char* buf,
                  uint64_t size,
                  off_t off);
    ssize_t
    write_to_file(const volumedriverfs::ObjectId& id,
                  const char* buf,
                  uint64_t size,
                  off_t off);

    ssize_t
    read_from_file(const volumedriverfs::FrontendPath& fname,
                   char* buf,
                   uint64_t size,
                   off_t off);

    ssize_t
    read_from_file(const volumedriverfs::ObjectId& id,
                   char *buf,
                   uint64_t size,
                   off_t off);

    template<typename T>
    void
    check_file(const T& entity,
               const std::string& pattern,
               uint64_t size,
               off_t off)
    {
        EXPECT_FALSE(pattern.empty()) << "fix your test";
        EXPECT_LT(0U, size);

        std::vector<char> buf(size);
        EXPECT_EQ(static_cast<ssize_t>(size),
                  read_from_file(entity, &buf[0], buf.size(), off));
        for (size_t i = 0; i < size; ++i)
        {
            EXPECT_EQ(pattern[i % pattern.size()], buf[i]) << "mismatch at offset " << i;
        }
    }

    // We can't use the public interface of FileSystem, as that needs a (per-thread)
    // fuse_context which we unsurprisingly don't have as we don't run fuse. Hence
    // the need for these wrappers (or alternatively befriending all tests, which
    // sounds even less attractive).
    // Add more of these as the need arises.
    int
    opendir(const volumedriverfs::FrontendPath& p,
            volumedriverfs::Handle::Ptr& handle);

    int
    releasedir(const volumedriverfs::FrontendPath& p,
               volumedriverfs::Handle::Ptr fi);

    int
    mknod(const volumedriverfs::FrontendPath& path,
          mode_t mode,
          dev_t rdev);

    int
    mknod(const volumedriverfs::ObjectId& parent_id,
          const std::string& name,
          mode_t mode);

    int
    unlink(const volumedriverfs::FrontendPath& path);

    int
    unlink(const volumedriverfs::ObjectId& parent_id,
           const std::string& name);

    int
    truncate(const volumedriverfs::FrontendPath& path,
             off_t size);

    int
    truncate(const volumedriverfs::ObjectId& id,
             off_t size);

    int
    rename(const volumedriverfs::FrontendPath& from,
           const volumedriverfs::FrontendPath& to,
           volumedriverfs::FileSystem::RenameFlags =
           volumedriverfs::FileSystem::RenameFlags::None);

    int
    rename(const volumedriverfs::ObjectId& from_parent_id,
           const std::string& from_name,
           const volumedriverfs::ObjectId& to_parent_id,
           const std::string& to_name,
           volumedriverfs::FileSystem::RenameFlags =
           volumedriverfs::FileSystem::RenameFlags::None);

    int
    open(const volumedriverfs::FrontendPath& path,
         volumedriverfs::Handle::Ptr& handle,
         int flags);

    int
    open(const volumedriverfs::ObjectId& id,
         volumedriverfs::Handle::Ptr& handle,
         mode_t flags);

    int
    release(const volumedriverfs::FrontendPath& path,
            volumedriverfs::Handle::Ptr handle);

    int
    release(volumedriverfs::Handle::Ptr handle);

    int
    write(const volumedriverfs::FrontendPath& path,
          const char* buf,
          uint64_t size,
          off_t off,
          volumedriverfs::Handle&);

    int
    write(volumedriverfs::Handle& h,
          const char *buf,
          uint64_t size,
          off_t off);

    int
    fsync(volumedriverfs::Handle&,
          bool datasync);

    int
    read(const volumedriverfs::FrontendPath& path,
         char* buf,
         uint64_t size,
         off_t off,
         volumedriverfs::Handle&);

    int
    read(volumedriverfs::Handle& h,
         char *buf,
         uint64_t size,
         off_t off);

    int
    getattr(const volumedriverfs::FrontendPath& path,
            struct stat& st);

    int
    getattr(const volumedriverfs::ObjectId& id,
            struct stat& st);

    int
    mkdir(const volumedriverfs::FrontendPath& path,
          mode_t mode);

    int
    mkdir(const volumedriverfs::ObjectId& parent_id,
          const std::string& name,
          mode_t mode);

    int
    rmdir(const volumedriverfs::FrontendPath& path);

    int
    rmdir(const volumedriverfs::ObjectId& id);

    int
    chmod(const volumedriverfs::FrontendPath& path,
          mode_t mode);

    int
    chmod(const volumedriverfs::ObjectId& id,
          mode_t mode);

    int
    chown(const volumedriverfs::FrontendPath& path,
          uid_t uid,
          gid_t gid);

    int
    chown(const volumedriverfs::ObjectId& id,
          uid_t uid,
          gid_t gid);

    int
    utimens(const volumedriverfs::FrontendPath& path,
            const struct timespec ts[2]);

    int
    utimens(const volumedriverfs::ObjectId& id,
            const struct timespec ts[2]);

    static void
    check_file_path_sanity(const volumedriverfs::FrontendPath& fpath);

    template<typename T>
    void
    verify_absence(const T& entity)
    {
        struct stat st;
        ASSERT_EQ(-ENOENT, getattr(entity, st));
    }

    template<typename T>
    void
    verify_presence(const T& entity)
    {
        struct stat st;
        ASSERT_EQ(0, getattr(entity, st));
    }

    template<typename T>
    void
    check_stat(const T& entity,
               uint64_t size,
               boost::optional<mode_t> maybe_mode = boost::none)
    {
        struct stat st;
        st.st_size = size + 1;
        st.st_mode = 0;

        ASSERT_EQ(0, getattr(entity, st));
        ASSERT_EQ(static_cast<ssize_t>(size), st.st_size);
        ASSERT_TRUE(S_ISREG(st.st_mode));

        if (maybe_mode)
        {
            ASSERT_EQ(*maybe_mode bitor S_IFREG, st.st_mode);
        }
    }

    template<typename T>
    void
    check_dir_stat(const T& entity,
                   boost::optional<mode_t> maybe_mode = boost::none)
    {
        struct stat st;
        st.st_size = 0;
        st.st_mode = 0;

        EXPECT_EQ(0, getattr(entity, st));
        EXPECT_TRUE(S_ISDIR(st.st_mode));

        if (maybe_mode)
        {
            EXPECT_EQ(*maybe_mode bitor S_IFDIR, st.st_mode);
        }
    }

    boost::optional<volumedriverfs::ObjectId>
    find_object(const volumedriverfs::FrontendPath& p);

    void
    wait_for_snapshot(const volumedriverfs::ObjectId& volume_id,
                      const std::string& snapshot_name,
                      uint32_t max_wait_secs = 120);

    volumedriverfs::ObjectRegistrationPtr
    find_registration(const volumedriverfs::ObjectId& id);

    bool
    is_registered(const volumedriverfs::ObjectId& id);

    void
    verify_registration(const volumedriverfs::ObjectId& id,
                        const volumedriverfs::NodeId& node_id);

    template<typename T>
    volumedriverfs::DirectoryEntryPtr
    find_in_volume_entry_cache(const T& entity)
    {
        VERIFY(fs_ != nullptr);
        return fs_->mdstore_.find_in_cache_(entity);
    }

    volumedriverfs::FrontendPath
    clone_path_to_volume_path(const volumedriverfs::FrontendPath& clone_path);

    std::shared_ptr<volumedriverfs::LocalNode>
    local_node(volumedriverfs::ObjectRouter& vrouter);

    std::shared_ptr<volumedriverfs::ClusterRegistry>
    cluster_registry(volumedriverfs::ObjectRouter& vrouter);

    bool
    is_mounted(const boost::filesystem::path& p);

    bool
    is_remote_mounted()
    {
        return is_mounted(remote_dir(topdir_));
    }

    void
    mount_remote();

    bool
    fork_and_exec_umount_(const boost::filesystem::path& mntpoint);

    void
    umount_remote();

    void
    wait_for_remote_();

    template<typename Param>
    void
    set_object_router_param_(const Param&);

    void
    set_volume_write_threshold(uint64_t wthresh);

    void
    set_volume_read_threshold(uint64_t rthresh);

    void
    set_file_write_threshold(uint64_t wthresh);

    void
    set_file_read_threshold(uint64_t rthresh);

    void
    set_backend_sync_timeout(const boost::chrono::milliseconds&);

    void
    set_lock_reaper_interval(uint64_t rthresh);

    void
    set_use_fencing(bool);

    void
    check_snapshots(const volumedriverfs::ObjectId& volume_id,
                    const std::vector<std::string>& expected_snapshots);

    template<typename T>
    void
    test_chown(const T& entity)
    {
        uid_t uid = getuid();
        gid_t gid = getgid();

        // The idea is to test changing the gid to a supplementary one, as that is
        // supposed to work without CAP_CHOWN.
        std::vector<gid_t> gids(1024); // just to be pretty sure.
        int ngroups = getgroups(gids.size(), gids.data());
        ASSERT_LT(1, ngroups) << "sorry, this test only works for users that are in at least 2 groups";

        gid_t* supp_gid = nullptr;
        for (auto i = 0; i < ngroups; ++i)
        {
            if (gids[i] != gid)
            {
                supp_gid = gids.data() + i;
                break;
            }
        }

        ASSERT_TRUE(supp_gid != nullptr);

        {
            struct stat st;
            memset(&st, 0x0, sizeof(st));

            EXPECT_EQ(0, getattr(entity, st));

            EXPECT_EQ(uid, st.st_uid);
            EXPECT_EQ(gid, st.st_gid);
        }

        EXPECT_EQ(0, chown(entity, uid, *supp_gid));

        {
            struct stat st;
            memset(&st, 0x0, sizeof(st));

            ASSERT_EQ(0, getattr(entity, st));

            EXPECT_EQ(uid, st.st_uid);
            EXPECT_EQ(*supp_gid, st.st_gid);
        }

        EXPECT_EQ(0, chown(entity, -1, -1));

        {
            struct stat st;
            memset(&st, 0x0, sizeof(st));

            ASSERT_EQ(0, getattr(entity, st));

            EXPECT_EQ(uid, st.st_uid);
            EXPECT_EQ(*supp_gid, st.st_gid);
        }
    }

    template<typename T>
    void
    test_chmod_file(const T& entity,
                    mode_t mode)
    {
        EXPECT_EQ(0, chmod(entity, mode));
        check_stat(entity, 0, mode);
    }

    template<typename T>
    void
    test_timestamps(const T& entity)
    {
        struct stat st;
        memset(&st, 0x0, sizeof(st));

        getattr(entity, st);

        struct timeval tv;
        ASSERT_EQ(0, gettimeofday(&tv, nullptr));

        EXPECT_NEAR(tv.tv_sec, st.st_atime, 1);
        EXPECT_NEAR(tv.tv_sec, st.st_ctime, 1);
        EXPECT_NEAR(tv.tv_sec, st.st_mtime, 1);

        struct timeval tv2 = tv;
        ASSERT_EQ(0, gettimeofday(&tv2, nullptr));

        while (tv2.tv_sec == tv.tv_sec)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ASSERT_EQ(0, gettimeofday(&tv2, nullptr));
        }

        EXPECT_NE(tv2.tv_sec, tv.tv_sec);

        EXPECT_EQ(0, utimens(entity, nullptr));
        getattr(entity, st);

        EXPECT_NEAR(tv.tv_sec, st.st_ctime, 1);
        EXPECT_NEAR(tv2.tv_sec, st.st_atime, 1);
        EXPECT_NEAR(tv2.tv_sec, st.st_mtime, 1);

        EXPECT_LT(st.st_ctime, st.st_atime);
        EXPECT_EQ(st.st_atime, st.st_mtime);

        struct timespec ts[2];
        ts[0].tv_sec = tv2.tv_sec + 3600;
        ts[0].tv_nsec = 0;
        ts[1].tv_sec = tv2.tv_sec + 7200;
        ts[1].tv_nsec = 0;

        EXPECT_EQ(0, utimens(entity, ts));
        getattr(entity, st);

        EXPECT_NEAR(tv.tv_sec, st.st_ctime, 1);
        EXPECT_EQ(ts[0].tv_sec, st.st_atime);
        EXPECT_EQ(ts[1].tv_sec, st.st_mtime);
    }

    size_t
    get_cluster_size(const volumedriverfs::ObjectId&) const;

    static boost::filesystem::path binary_path_;

    std::unique_ptr<volumedriverfs::FileSystem> fs_;

    // pstreams are quite pointless for our use case as we don't intend
    // to communicate with the child via std{in,out,err}. A chatty child
    // (I'm sorry to say that our child can be *very* chatty) could hence fill up
    // the write end of a pipe that we never empty, leading to the child blocking
    // at inconvenient times.
    // Let's fall back to plain fork / exec / waitpid instead.
    pid_t remote_pid_;

private:
    DECLARE_LOGGER("FileSystemTestBase");

    template<typename ...A>
    static int
    fs_convert_exceptions(volumedriverfs::FileSystem& fs_,
                          void (volumedriverfs::FileSystem::*func)(A ...args),
                          A ...args) throw()
    {
        int ret = 0;

        try
        {
            ((&fs_)->*func)(std::forward<A>(args)...);
        }
        catch (volumedriverfs::GetAttrOnInexistentPath&)
        {
            ret = -ENOENT;
        }
        catch (volumedriverfs::InternalNameException& e)
        {
            ret = -EINVAL;
        }
        catch (volumedriver::VolManager::VolumeDoesNotExistException& e)
        {
            ret = -ENOENT;
        }
        catch (volumedriverfs::ObjectNotRegisteredException& e)
        {
            ret = -ENOENT;
        }
        catch (volumedriverfs::ConflictingUpdateException& e)
        {
            ret = -EAGAIN;
        }
        catch (std::system_error& e)
        {
            ret = -e.code().value();
        }
        catch (boost::system::system_error& e)
        {
            ret = -e.code().value();
        }
        catch (std::error_code& e)
        {
            ret = -e.value();
        }
        catch (boost::system::error_code& e)
        {
            ret = -e.value();
        }
        catch (fungi::IOException& e)
        {
            ret = -e.getErrorCode();
            if (ret >= 0)
            {
                ret = -EIO;
            }
        }
        catch (...)
        {
            ret = -EIO;
        }

        //FIXME What to do with the fs_ cache ?
        //if (ret < 0)
        //{
            //fs_.drop_from_cache(path/id);
        //}

        return ret;
    }
};

}

#endif // !FILE_SYSTEM_TEST_SETUP_H_

// Local Variables: **
// mode: c++ **
// End: **
