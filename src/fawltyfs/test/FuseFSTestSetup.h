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
