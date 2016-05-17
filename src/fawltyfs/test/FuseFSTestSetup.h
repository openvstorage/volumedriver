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

#ifndef FUSE_FS_TEST_SETUP_H_
#define FUSE_FS_TEST_SETUP_H_

#include <boost/filesystem.hpp>
#include <boost/utility.hpp>

#include <gtest/gtest.h>

#include <fawltyfs/FileSystem.h>
#include <youtils/Logging.h>

namespace fawltyfstest
{

class FuseFSTestSetup
    : public testing::Test
    , private boost::noncopyable
{
public:
    explicit FuseFSTestSetup(const std::string& testname);

    virtual ~FuseFSTestSetup();

protected:
    const std::string testname_;
    fuse_fs* fuse_fs_;
    boost::filesystem::path path_;
    std::shared_ptr<fawltyfs::FileSystem> fs_;

private:
    DECLARE_LOGGER("FuseFSTestSetup")

};

}

#endif // !FUSE_FS_TEST_SETUP_H_

// Local Variables: **
// mode: c++ **
// End: **
