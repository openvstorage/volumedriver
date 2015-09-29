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

#include "DirectIORule.h"
#include "ErrorRules.h"
#include "Exception.h"
#include "FileSystemCall.h"
// This will clash with volumedriver/youtils should it ever be used together with
// fawltyfs.
#include <youtils/Logging.h>

#include <cstdlib>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

#include <boost/thread/mutex.hpp>
#include <boost/filesystem.hpp>
#include <youtils/Assert.h>
#define FUSE_USE_VERSION 26

#include <fuse.h>

struct fuse_operations;

namespace fawltyfstest
{
class FuseFSTestSetup;
}

namespace fawltyfs
{

DECLARE_EXCEPTION(ConfigException, Exception);
DECLARE_EXCEPTION(ManagementException, Exception);

class FileSystem
{
    // XXX: this - and the string args to the constructor + init_ops_ below -
    // needs to go
    friend class fawltyfstest::FuseFSTestSetup;

public:
    FileSystem(const boost::filesystem::path& basedir,
               const boost::filesystem::path& frontend_dir,
               const std::vector<std::string>& fuse_args = default_fuse_args,
               const char* fsname = "fawltyfs");

    ~FileSystem();

    FileSystem(const FileSystem&) = delete;

    FileSystem&
    operator=(const FileSystem&) = delete;

    void
    mount();

    void
    umount();

    bool
    mounted() const;


public:
    void
    addDelayRule(DelayRulePtr rule);

    void
    removeDelayRule(rule_id rule_id);

    void
    listDelayRules(delay_rules_list_t& l);

    void
    addFailureRule(FailureRulePtr rule);

    void
    removeFailureRule(rule_id rule_id);

    void
    listFailureRules(failure_rules_list_t& l);

    void
    addDirectIORule(DirectIORulePtr rule);

    void
    removeDirectIORule(rule_id rule_id);

    void
    listDirectIORules(direct_io_rules_list_t& l);

    static const std::vector<std::string>
    default_fuse_args;

private:

    private:
    static int
    getattr(const char* path,
            struct stat* st);

    static int
    access(const char* path,
           int mask);

    static int
    readlink(const char* path,
             char* buf,
             size_t size);

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
            struct fuse_file_info* fi);

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
    symlink(const char* from,
            const char* to);

    static int
    rename(const char* from,
           const char* to);

    static int
    link(const char* from,
         const char* to);

    static int
    chmod(const char* path, mode_t mode);

    static int
    chown(const char* path,
          uid_t uid,
          gid_t gid);

    static int
    truncate(const char* path,
             off_t size);

    static int
    ftruncate(const char* path,
              off_t size,
              fuse_file_info* fi);

    static int
    utimens(const char* path,
            const struct timespec ts[2]);

    static int
    open(const char* path,
         struct fuse_file_info* fi);

    static int
    read(const char* path,
         char* buf,
         size_t size,
         off_t off,
         struct fuse_file_info* fi);

    static int
    write(const char* path,
          const char* buf,
          size_t size,
          off_t off,
          struct fuse_file_info* fi);

    static int
    statfs(const char* path,
           struct statvfs* stbuf);

    static int
    release(const char* path,
            struct fuse_file_info* fi);

    static int
    fsync(const char* path,
          int datasync,
          struct fuse_file_info* fi);

    DECLARE_LOGGER("FileSystem");

    const std::string name_;
    const boost::filesystem::path backend_dir_;
    const boost::filesystem::path frontend_dir_;
    const std::vector<std::string> fuse_args_;

    // protects the rules and thread_ + fuse_
    typedef std::mutex lock_type;
    mutable lock_type lock_;

    fuse* fuse_;
    std::unique_ptr<std::thread> thread_;

    typedef std::map<rule_id, DelayRulePtr> delay_rules_t;
    delay_rules_t delay_rules_;

    typedef std::map<rule_id, FailureRulePtr> failure_rules_t;
    failure_rules_t failure_rules_;

    typedef std::map<rule_id, DirectIORulePtr> direct_io_rules_t;
    direct_io_rules_t direct_io_rules_;

    void
    init_ops_(fuse_operations& ops) const;

    void
    run_(bool multithreaded, char* mountpoint);

    static inline FileSystem*
    instance_()
    {
        fuse_context* ctx = fuse_get_context();
        VERIFY(ctx);
        VERIFY(ctx->private_data);
        return reinterpret_cast<FileSystem*>(ctx->private_data);
    }

    int
    getattr_(const std::string& path,
             struct stat* st);

    int
    access_(const std::string& path,
            int mask);

    int
    readlink_(const std::string& path,
              char* buf,
              size_t size);

    int
    opendir_(const std::string& path,
             fuse_file_info* fi);

    int
    releasedir_(const std::string& path,
                fuse_file_info* fi);

    int
    readdir_(const std::string& path,
             void* buf,
             fuse_fill_dir_t filler,
             off_t offset,
             struct fuse_file_info* fi);

    int
    mknod_(const std::string& path,
           mode_t mode,
           dev_t rdev);

    int
    mkdir_(const std::string& path,
           mode_t mode);

    int
    unlink_(const std::string& path);

    int
    rmdir_(const std::string& path);

    int
    symlink_(const std::string& from,
             const std::string& to);

    int
    rename_(const std::string& from,
            const std::string& to);

    int
    link_(const std::string& from,
          const std::string& to);

    int
    chmod_(const std::string& path, mode_t mode);

    int
    chown_(const std::string& path,
           uid_t uid,
           gid_t gid);

    int
    truncate_(const std::string& path,
              off_t size);

    int
    ftruncate_(const std::string& path,
               off_t size,
               struct fuse_file_info* fi);

    int
    utimens_(const std::string& path,
             const struct timespec ts[2]);

    int
    open_(const std::string& path,
          struct fuse_file_info* fi);

    int
    read_(const std::string& path,
          char* buf,
          size_t size,
          off_t off,
          struct fuse_file_info* fi);

    int
    write_(const std::string& path,
           const char* buf,
           size_t size,
           off_t off,
           struct fuse_file_info* fi);

    int
    statfs_(const std::string& path,
            struct statvfs* stbuf);

    int
    release_(const std::string& path,
             struct fuse_file_info* fi);

    int
    fsync_(const std::string& path,
           int datasync,
           struct fuse_file_info* fi);

    // xattr ops are left unimplemented for now.

    int
    maybeInjectError_(const std::string& path,
                      const FileSystemCall call);

    void
    maybeInjectDelay_(const std::string& path,
                      const FileSystemCall call);

    int
    maybeInjectFailure_(const std::string& path,
                        const FileSystemCall call);

    bool
    useDirectIO_(const std::string& path) const;
};

struct FileSystemUmounter
{
    void
    operator()(FileSystem* f)
    {
        f->umount();
        delete f;
    }

};



typedef std::unique_ptr<FileSystem, FileSystemUmounter> FileSystemManager;

}

#endif // !FILESYSTEM_H_

// Local Variables: **
// mode: c++ **
// End: **
