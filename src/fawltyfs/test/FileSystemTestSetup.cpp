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
