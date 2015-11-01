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

#ifndef FILESYSTEM_TEST_SETUP_H_
#define FILESYSTEM_TEST_SETUP_H_

#include "../FileSystem.h"
#include <youtils/Logging.h>

#include <boost/filesystem.hpp>
#include <boost/utility.hpp>

#include <gtest/gtest.h>

namespace fawltyfstest
{
namespace ffs = fawltyfs;

class FileSystemTestSetup
    : public testing::Test
    , private boost::noncopyable
{
public:
    explicit FileSystemTestSetup(const std::string& testname);

    virtual ~FileSystemTestSetup();

protected:
    const std::string testname_;
    pid_t fs_pid_;
    const boost::filesystem::path sourcedir_;
    const boost::filesystem::path mntpoint_;

private:
    DECLARE_LOGGER("FileSystemTestSetup");

    void
    mount_filesystem_();

    void
    umount_filesystem_();
};

}

#endif  // !FILESYSTEM_TEST_SETUP_H_

// Local Variables: **
// mode: c++ **
// End: **
