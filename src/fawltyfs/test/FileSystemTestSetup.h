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
