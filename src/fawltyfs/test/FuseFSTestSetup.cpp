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

#include "FuseFSTestSetup.h"

#include <youtils/Assert.h>
#include <youtils/FileUtils.h>
#include <fuse.h>

namespace fawltyfstest
{

namespace ffs = fawltyfs;
namespace fs = boost::filesystem;

FuseFSTestSetup::FuseFSTestSetup(const std::string& testname)
    : testname_(testname)
    , fuse_fs_(0)
    , path_(youtils::FileUtils::temp_path() / testname_)
    , fs_(new ffs::FileSystem(path_, "", std::vector<std::string>()))
{
    // push to SetUp?
    fs::remove_all(path_);
    fs::create_directories(path_);
    struct fuse_operations ops;
    fs_->init_ops_(ops);
    fuse_fs_ = fuse_fs_new(&ops, sizeof(ops), fs_.get());
    VERIFY(fuse_fs_);
    //    int val = ::fuse_create_context_key();
    // VERIFY(val == 0);

}

FuseFSTestSetup::~FuseFSTestSetup()
{
    // push to TearDown?
    try
    {
        free(fuse_fs_);
        fuse_fs_ = 0;
        fs_.reset();
        fs::remove_all(path_);
    }
    catch (std::exception& e)
    {
        LOG_FATAL("Caught exception in destructor: " << e.what());
    }
    catch (...)
    {
        LOG_FATAL("Caught unknown exception in destructor");
    }
}

}
// Local Variables: **
// mode: c++ **
// End: **
