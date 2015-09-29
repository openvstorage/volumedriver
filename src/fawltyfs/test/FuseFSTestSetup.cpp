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
