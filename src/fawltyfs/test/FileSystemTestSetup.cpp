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

#include "FileSystemTestSetup.h"

#include <youtils/FileUtils.h>

namespace fawltyfstest
{

namespace fs = boost::filesystem;

FileSystemTestSetup::FileSystemTestSetup(const std::string& testname)
    : testname_(testname)
    , fs_pid_(-1)
    , sourcedir_(youtils::FileUtils::temp_path() / (testname_ + "_source"))
    , mntpoint_(youtils::FileUtils::temp_path() / (testname_ + "_mnt"))
{
    fs::remove_all(sourcedir_);
    fs::remove_all(mntpoint_);

    fs::create_directories(sourcedir_);
    fs::create_directories(mntpoint_);

    mount_filesystem_();
}

FileSystemTestSetup::~FileSystemTestSetup()
{
    try
    {
        umount_filesystem_();
        fs::remove_all(sourcedir_);
        fs::remove_all(mntpoint_);
    }
    catch (std::exception& e)
    {
        LOG_FATAL("caught exception: " << e.what());
    }
    catch (...)
    {
        LOG_FATAL("caught unknown exception");
    }
}

void
FileSystemTestSetup::mount_filesystem_()
{
}

void
FileSystemTestSetup::umount_filesystem_()
{
}

}

// Local Variables: **
// mode: c++ **
// End: **
