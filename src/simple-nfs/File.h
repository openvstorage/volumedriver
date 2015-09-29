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

#ifndef SIMPLE_NFS_FILE_H_
#define SIMPLE_NFS_FILE_H_

#include <memory>

#include <boost/filesystem.hpp>

#include <youtils/Logging.h>
#include <youtils/FileDescriptor.h>

struct nfs_context;
struct nfsfh;

namespace simple_nfs
{

class Mount;

class File
{
public:
    File(Mount& mnt,
         const boost::filesystem::path& p,
         youtils::FDMode mode,
         CreateIfNecessary create_if_necessary = CreateIfNecessary::F);

    ~File() = default;

    File(const File&) = default;

    File&
    operator=(const File&) = default;

    size_t
    pread(void* buf,
          size_t count,
          off_t off);

    using ReadCallback = std::function<void(ssize_t res)>;

    void
    pread(void* buf,
          size_t count,
          off_t off,
          ReadCallback rcb);

    size_t
    pwrite(const void* buf,
           size_t count,
           off_t off);

    using WriteCallback = std::function<void(ssize_t res)>;

    void
    pwrite(const void* buf,
           size_t count,
           off_t off,
           WriteCallback wcb);
    void
    fsync();

    void
    truncate(uint64_t len);

    struct stat
    stat();

private:
    DECLARE_LOGGER("SimpleNfsFile");

    std::shared_ptr<nfs_context> ctx_;
    std::shared_ptr<nfsfh> fh_;

    template<typename... A>
    int
    convert_errors_(const char* desc,
                    int (*nfs_fun)(nfs_context*, nfsfh*, A...),
                    A...);
};

}

#endif // !SIMPLE_NFS_FILE_H_
