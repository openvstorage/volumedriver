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

#include "../TestBase.h"
#include "../FileUtils.h"
#include "../FileDescriptor.h"

#include <mutex>

#include <boost/filesystem/fstream.hpp>

namespace youtilstest
{
using namespace youtils;

class LowLevelFileTest : public TestBase
{
public:
    LowLevelFileTest()
        :directory_(getTempPath("LowLevelFileTest"))
    {}

    void
    SetUp()
    {
        fs::remove_all(directory_);
        fs::create_directories(directory_);
    }

    void
    TearDown()
    {
        fs::remove_all(directory_);
    }

protected:
    fs::path directory_;
};

// the same as SimpleIO
using TestFile = FileDescriptor;


TEST_F(LowLevelFileTest, mixedBag)
{
    fs::path p(directory_ / "testfile");
    EXPECT_THROW(TestFile f(p, FDMode::Write),
                 FileDescriptorException);

    std::vector<uint8_t> buf1(2048);
    memset(&buf1[0], 0xff, buf1.size());

    std::vector<uint8_t> buf2(4096);
    memset(&buf2[0], 0x42, buf2.size());

    std::vector<uint8_t> out(4096);

    {
        TestFile f(p, FDMode::Write, CreateIfNecessary::T);

        EXPECT_EQ(buf1.size(), f.write(&buf1[0], buf1.size()));
        EXPECT_THROW(f.read(&out[0], out.size()),
                     FileDescriptorException);
    }

    {
        TestFile g(p, FDMode::ReadWrite, CreateIfNecessary::F);

        g.seek(0, Whence::SeekEnd);
        EXPECT_EQ(buf2.size(), g.write(&buf2[0], buf2.size()));
        EXPECT_EQ(buf1.size(), g.pwrite(&buf1[0], buf1.size(), g.tell()));
    }

    {
        TestFile h(p, FDMode::Read, CreateIfNecessary::F);
        EXPECT_THROW(h.pwrite(&buf2[0], buf2.size(), 0),
                     FileDescriptorException);

        h.seek(buf1.size() + buf2.size(), Whence::SeekSet);
        memset(&out[0], 0x0, out.size());
        EXPECT_EQ(buf1.size(), h.read(&out[0], buf1.size()));
        EXPECT_EQ(0, memcmp(&buf1[0], &out[0], buf1.size()));

        h.seek(-(2 * buf1.size() + buf2.size()), Whence::SeekCur);
        memset(&out[0], 0x0, out.size());
        EXPECT_EQ(buf1.size(), h.read(&out[0], buf1.size()));
        EXPECT_EQ(0, memcmp(&buf1[0], &out[0], buf1.size()));

        memset(&out[0], 0x0, out.size());
        EXPECT_EQ(buf2.size(), h.pread(&out[0], buf2.size(), buf1.size()));
        EXPECT_EQ(0, memcmp(&buf2[0], &out[0], buf2.size()));

        EXPECT_EQ(buf1.size(),
                  h.pread(&out[0], out.size(), buf1.size() + buf2.size()));

        h.seek(-buf1.size(), Whence::SeekEnd);
        EXPECT_EQ(buf1.size(),
                  h.read(&out[0], out.size()));
    }

    EXPECT_EQ(2 * buf1.size() + buf2.size(), fs::file_size(p));
}

TEST_F(LowLevelFileTest, seek1)
{
    fs::path p = directory_ / "testfile";
    EXPECT_THROW(TestFile f(p, FDMode::Read),
                 FileDescriptorException);
    FileUtils::touch(p);
    TestFile f(p, FDMode::Read);
    EXPECT_THROW(f.seek(-25, Whence::SeekCur),
                 FileDescriptorException);
    EXPECT_EQ(f.tell(),0);
    f.seek(0, Whence::SeekEnd);
    EXPECT_EQ(f.tell(),0);
    fs::ofstream of(p);
    of << "Some Bytes";
    of.close();
    f.seek(0, Whence::SeekEnd);
    EXPECT_EQ(f.tell(), 10);
    f.seek(-4, Whence::SeekCur);
    EXPECT_EQ(f.tell(), 6);
}

TEST_F(LowLevelFileTest, locking)
{
    const fs::path p(directory_ / "lockfile");
    TestFile f(p,
               FDMode::Write,
               CreateIfNecessary::T);

    std::unique_lock<decltype(f)> u(f);
    ASSERT_TRUE(static_cast<bool>(u));

    TestFile g(p,
               FDMode::Write);

    std::unique_lock<decltype(g)> v(g,
                                    std::try_to_lock);
    ASSERT_FALSE(static_cast<bool>(v));
}

}

// Local Variables: **
// mode: c++ **
// End: **
