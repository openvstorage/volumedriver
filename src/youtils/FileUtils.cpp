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

#include "FileUtils.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <boost/filesystem/fstream.hpp>
#include "FileDescriptor.h"
#include "Assert.h"
#include "ScopeExit.h"
#include <sys/stat.h>
#include <fcntl.h>

namespace youtils
{

// Returns a path that points to an existing file
fs::path
FileUtils::create_temp_file(const fs::path& path,
                            const std::string& name)
{
    fs::path p = path / name;
    return create_temp_file(p);
}

// Returns a path that points to an existing file file
fs::path
FileUtils::create_temp_file(const fs::path& path)
{
    std::string p = path.string() + std::string("XXXXXX");
    VERIFY(p.size() < PATH_MAX);
    char temp[PATH_MAX];
    strcpy(temp, p.c_str());
    int tempfd = mkstemp(temp);
    if(tempfd < 0)
    {
        int err = errno;
        LOG_ERROR("Failed to create temp file " << temp << ": " << strerror(err));
        throw fungi::IOException("Failed to create temp file",
                                 temp,
                                 err);
    }
    ::close(tempfd);
    return fs::path(temp);
}

fs::path
FileUtils::create_temp_file_in_temp_dir(const std::string& name)
{
    return create_temp_file(temp_path()  / name);
}

fs::path
FileUtils::create_temp_dir(const fs::path& dir,
                           const std::string& name)
{
    fs::path tmp = dir / fs::path(name + std::string("XXXXXX"));

    VERIFY(tmp.string().size() < PATH_MAX);
    char temp[PATH_MAX];
    strcpy(temp, tmp.string().c_str());
    char* res = mkdtemp(temp);
    if(res)
    {
        return fs::path(res);
    }
    else
    {
        int err = errno;
        LOG_ERROR("Failed to create temp dir " << temp << ": " << strerror(err));
        throw fungi::IOException("Failed to create temp directory");
    }
}

fs::path
FileUtils::create_temp_dir_in_temp_dir(const std::string& name)
{
    return create_temp_dir(temp_path(),
                           name);
}

void
FileUtils::touch(const fs::path& path)
{
    if(not fs::exists(path))
    {
        fs::ofstream f(path);
        f.open(path);
        f.close();
    }
}

void
FileUtils::make_paths(const std::vector<std::string>& in,
                     std::vector<fs::path>& out)
{

    for(std::vector<std::string>::const_iterator i = in.begin();
        i != in.end();
        ++i)
    {
        out.push_back(*i);
    }
}

void
FileUtils::truncate(const fs::path& p,
                    const uintmax_t new_size)
{
    const char *cpath = p.string().c_str();

    uintmax_t old_size = boost::filesystem::file_size(p);
    if (old_size < new_size) {
        LOG_ERROR("" << p << ": truncating from " << old_size << " to " <<
                  new_size << " is not supported");
        throw fungi::IOException("Truncating to larger size is not supported",
                                 cpath, EINVAL);
    }

    int ret = ::truncate(cpath, new_size);
    if (ret < 0) {
        LOG_ERROR("" << p << ": truncate failed: " << strerror(errno));
        throw fungi::IOException("Truncate failed", cpath, errno);
    }
}

fs::path
FileUtils::temp_path()
{
    const char* tmp = getenv("TEMP");
    if(not tmp)
        tmp = getenv("TMP");
    if(not tmp)
        tmp = "/tmp/";
    fs::path tmp_path(tmp);

    if(not fs::exists(tmp_path) or
       not fs::is_directory(tmp_path))
    {
        throw fungi::IOException("Could not get temp path");
    }
    return tmp_path;
}

void
FileUtils::ensure_directory(const fs::path& path)
{
    if(fs::exists(path) and
       not fs::is_directory(path))
    {
        LOG_ERROR("path " << path << " exists but is not a directory. Not Good!");

        throw NotADirectoryException(path.string());
    }
    else
    {
        try
        {
            fs::create_directories(path);
        }
        catch(std::exception& e)
        {
            LOG_ERROR("Could not create directory " << path);
            throw NotADirectoryException(std::string(e.what()) +
                                         std::string(", ") +
                                         path.string());
        }

    }
}

void
FileUtils::ensure_file(const fs::path& path,
                       const uint64_t size,
                       const ForceFileSize)
{
    if(fs::exists(path))
    {
        if(fs::is_directory(path))
        {
            LOG_ERROR("path " << path << " exists but is not a file. Bummer.");
            throw NotAFileException(path.string());
        }
    }
    if(not fs::exists(path))
    {
        fs::create_directories(path.parent_path());
        int fd = -1;
        fd = open(path.string().c_str(),
                  O_RDWR| O_CREAT | O_EXCL,
                  S_IWUSR|S_IRUSR);
        if(fd < 0)
        {
            LOG_ERROR("Couldn't open file " << path << ", " << strerror(errno));
            throw fungi::IOException("Couldn't open file",
                                     path.string().c_str(),
                                     errno);
        }
        auto on_exit_1 = make_scope_exit([fd](void){close(fd);});

        int ret = posix_fallocate(fd, 0, size);

        if (ret != 0)
        {
            LOG_ERROR("Could not allocate file " << path << " with size " << size << ", " << strerror(ret));
            throw fungi::IOException("Could not allocate file",
                                     path.string().c_str(),
                                     ret);
        }
        fchmod(fd,00600);

    }

}

/*
From the libc documentation:
     One useful feature of `rename' is that the meaning of NEWNAME
     changes "atomically" from any previously existing file by that
     name to its new meaning (i.e., the file that was called OLDNAME).
     There is no instant at which NEWNAME is non-existent "in between"
     the old meaning and the new meaning.  If there is a system crash
     during the operation, it is possible for both names to still
     exist; but NEWNAME will always be intact if it exists at all.
*/

void
FileUtils::syncAndRename(const fs::path& old_file,
                         const fs::path& new_file)
{
    {
        FileDescriptor f(old_file.string().c_str(), FDMode::Read);
        f.sync();
    }

    fs::rename(old_file,
               new_file);
}

void
FileUtils::safe_copy(const fs::path& src,
                     const fs::path& dst,
                     const SyncFileBeforeRename sync_before_rename)
{
    const fs::path tmp = create_temp_file(dst);
    boost::system::error_code ec;
    fs::copy_file(src,
                  tmp,
                  fs::copy_option::overwrite_if_exists,
                  ec);
    if(ec)
    {
        LOG_ERROR("copy operation returned " << ec.value());
        if(ec == boost::system::errc::errc_t::no_such_file_or_directory)
        {
            throw CopyNoSourceException("Could not safe copy file, no source");
        }
        else
        {
            throw CopyException("Could not copy file");
        }
    }

    if(T(sync_before_rename))
    {
        FileDescriptor f(tmp.string().c_str(), FDMode::Read);
        f.sync();
    }
    fs::rename(tmp,
               dst);
}

void
FileUtils::safe_copy(const std::string& istr,
                     const fs::path& dst,
                     const SyncFileBeforeRename sync_before_rename)
{
    const fs::path tmp = create_temp_file(dst);
    fs::ofstream o_tmp(tmp);
    o_tmp.exceptions(fs::ofstream::failbit bitor fs::ofstream::badbit);
    o_tmp << istr;
    o_tmp.flush();
    o_tmp.close();

    if (T(sync_before_rename))
    {
        FileDescriptor f(tmp.string().c_str(), FDMode::Read);
        f.sync();
    }
    fs::rename(tmp,
               dst);
}

void
FileUtils::statvfs_(const fs::path& path,
                    struct statvfs* st)
{
    int ret = ::statvfs(path.string().c_str(), st);
    if (ret < 0) {
        LOG_ERROR("Failed to determine size of filesystem for " << path <<
                  ": " << strerror(errno));
        throw fungi::IOException("Failed to determine size of filesystem",
                                 path.string().c_str(), errno);
    }
}

uint64_t
FileUtils::filesystem_size(const fs::path& path)
{
    struct statvfs st;

    statvfs_(path, &st);

    return st.f_frsize * st.f_blocks;
}

uint64_t
FileUtils::filesystem_free_size(const fs::path& path)
{
    struct statvfs st;

    statvfs_(path, &st);

    // this is the free space available for non-root users, f_bfree contains
    // the free space for root. better not go there.
    return st.f_bavail * st.f_frsize;
}

CheckSum
FileUtils::calculate_checksum(const fs::path& path)
{
    FileDescriptor f(path, FDMode::Read);
    CheckSum chksum;
    std::vector<uint8_t> buf(::sysconf(_SC_PAGESIZE));

    while (true)
    {
        ssize_t ret = f.read(&buf[0], buf.size());
        if (ret == 0)
        {
            break;
        }
        chksum.update(&buf[0], ret);
    }

    // f.close();
    return chksum;
}

CheckSum
FileUtils::calculate_checksum(const fs::path& path,
                               off_t offset)
{
    FileDescriptor f(path, FDMode::Read);
    CheckSum chksum;
    const off_t bufsize = ::sysconf(_SC_PAGESIZE);
    std::vector<uint8_t> buf(bufsize);

    while(offset > 0)
    {
        const unsigned to_read = std::min(offset, bufsize);

        ssize_t ret = f.read(&buf[0], to_read);
        if (ret != to_read)
        {
            throw CheckSumFileNotLargeEnough("checksum with offset, file not large enough");
        }

        chksum.update(&buf[0], to_read);
        offset -= to_read;
    }
    VERIFY(offset == 0);
    // f.close();
    return chksum;
}

void
FileUtils::removeFileNoThrow(const fs::path& path)
{
    try
    {
        if(fs::exists(path))
        {
            fs::remove(path);
        }

    }
    catch(std::exception& e)
    {
        LOG_WARN("Could not remove " << path << " " << e.what() << " -- ignoring");
    }
    catch(...)
    {
        LOG_WARN("Could not remove " << path << " -- ignoring");
    }
}

void
FileUtils::checkDirectoryEmptyOrNonExistant(const fs::path& path)
{
    if(fs::exists(path))
    {
        if(not fs::is_directory(path))
        {
            LOG_ERROR("Not a directory" <<  path);
            throw fungi::IOException("Directory not empty.");
        }

        else if(not fs::is_empty(path))
        {
            LOG_ERROR("Directory not empty: " << path);
            throw fungi::IOException("Directory not empty.");
        }
    }
    else
    {
        fs::create_directories(path);
    }
}

void
FileUtils::removeAllNoThrow(const fs::path& path)
{
    try
    {
        fs::remove_all(path);
    }
    catch(std::exception& e)
    {
        LOG_WARN("Could not remove " << path << " " << e.what() << " -- ignoring");
    }
    catch(...)
    {
        LOG_WARN("Could not remove " << path << " -- ignoring");
    }
}
}


// Local Variables: **
// End: **
