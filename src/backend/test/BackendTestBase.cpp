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

#include "BackendTestBase.h"
#include <youtils/FileDescriptor.h>
#include <youtils/FileUtils.h>
#include "../BackendException.h"

namespace backend
{

namespace fs = boost::filesystem;
namespace yt = youtils;

using youtils::FileDescriptor;
using youtils::FileUtils;
using youtils::FDMode;


BackendTestBase::BackendTestBase(const std::string& name)
    : BackendTestSetup()
    , path_(youtilstest::TestBase::getTempPath(name))
{}

void
BackendTestBase::SetUp()
{
    fs::remove_all(path_);
    fs::create_directories(path_);
    initialize_connection_manager();
}


void
BackendTestBase::TearDown()
{
    fs::remove_all(path_);
    uninitialize_connection_manager();
}

bool
BackendTestBase::verifyTestFile(const fs::path& p,
                                size_t expected_size,
                                const std::string& pattern,
                                const yt::CheckSum* expected_chksum)
{
    FileDescriptor sio(p, FDMode::Read);
    yt::CheckSum chksum;

    if (fs::file_size(p) != expected_size)
    {
        return false;
    }

    for (size_t i = 0; i < expected_size;)
    {
        size_t rsize = std::min(pattern.size(), expected_size - i);
        std::vector<char> buf(rsize);
        EXPECT_EQ(rsize, sio.read(&buf[0], rsize));
        if (strncmp(&buf[0], pattern.c_str(), rsize) != 0)
        {
            return false;
        }
        chksum.update(&buf[0], rsize);
        i += rsize;
    }

    if (expected_chksum != 0 and *expected_chksum != chksum)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool
BackendTestBase::retrieveAndVerify(const fs::path& p,
                                   size_t size,
                                   const std::string& pattern,
                                   const yt::CheckSum* chksum,
                                   BackendConnectionManagerPtr bc,
                                   const std::string& objname,
                                   const Namespace& ns)
{
    auto bi = bc->getConnection();
    bi->read(ns,
             p,
             objname,
             InsistOnLatestVersion::T);
    return verifyTestFile(p,
                          size,
                          pattern,
                          chksum);
}

yt::CheckSum
BackendTestBase::createAndPut(const fs::path& p,
                              size_t size,
                              const std::string& pattern,
                              BackendConnectionManagerPtr bc,
                              const std::string& objname,
                              const Namespace& ns,
                              const OverwriteObject overwrite)
{
    auto bi = bc->getConnection();
    const yt::CheckSum chksum(createTestFile(p, size, pattern));
    bi->write(ns,
              p,
              objname,
              overwrite,
              &chksum);
    return chksum;
}

yt::CheckSum
BackendTestBase::createPutAndVerify(const fs::path& p,
                                    size_t size,
                                    const std::string& pattern,
                                    BackendConnectionManagerPtr bc,
                                    const std::string& objname,
                                    const Namespace& ns,
                                    const OverwriteObject overwrite)
{
    auto bi = bc->getConnection();
    const yt::CheckSum chksum(createAndPut(p,
                                           size,
                                           pattern,
                                           bc,
                                           objname,
                                           ns,
                                           overwrite));

    EXPECT_TRUE(bi->objectExists(ns,
                                 objname));
    EXPECT_EQ(size, bi->getSize(ns,
                                objname));
    try
    {
        yt::CheckSum cs = bi->getCheckSum(ns,
                                          objname);
        EXPECT_EQ(cs, chksum);
    }
    catch(const BackendNotImplementedException&)
    {}

    return chksum;
}

yt::CheckSum
BackendTestBase::createTestFile(const fs::path& p,
                                size_t size,
                                const std::string& pattern)
{
    FileDescriptor sio(p, FDMode::Write, CreateIfNecessary::T);
    yt::CheckSum chksum;

    for (size_t i = 0; i < size;)
    {
        const size_t wsize = std::min(pattern.size(), size - i);
        EXPECT_EQ(wsize, sio.write(pattern.c_str(), wsize));
        chksum.update(pattern.c_str(), wsize);
        i += wsize;
    }

    EXPECT_TRUE(chksum == FileUtils::calculate_checksum(p));

    return chksum;
}

}
// Local Variables: **
// mode: c++ **
// End: **
