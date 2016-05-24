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

#include "../TestBase.h"
#include "../FileUtils.h"
#include "../FileDescriptor.h"
#include "../ScopeExit.h"

#include <sys/mman.h>

#include <mutex>

#include <boost/filesystem/fstream.hpp>

namespace youtilstest
{
using namespace youtils;

class FileDescriptorTest
    : public TestBase
{
public:
    FileDescriptorTest()
        :directory_(getTempPath("FileDescriptorTest"))
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

    static int
    get_fd(FileDescriptor& f)
    {
        return f.fd_;
    }

protected:
    fs::path directory_;
};

TEST_F(FileDescriptorTest, mixedBag)
{
    fs::path p(directory_ / "testfile");
    EXPECT_THROW(FileDescriptor f(p, FDMode::Write),
                 FileDescriptorException);

    std::vector<uint8_t> buf1(2048);
    memset(&buf1[0], 0xff, buf1.size());

    std::vector<uint8_t> buf2(4096);
    memset(&buf2[0], 0x42, buf2.size());

    std::vector<uint8_t> out(4096);

    {
        FileDescriptor f(p, FDMode::Write, CreateIfNecessary::T);

        EXPECT_EQ(buf1.size(), f.write(&buf1[0], buf1.size()));
        EXPECT_THROW(f.read(&out[0], out.size()),
                     FileDescriptorException);
    }

    {
        FileDescriptor g(p, FDMode::ReadWrite, CreateIfNecessary::F);

        g.seek(0, Whence::SeekEnd);
        EXPECT_EQ(buf2.size(), g.write(&buf2[0], buf2.size()));
        EXPECT_EQ(buf1.size(), g.pwrite(&buf1[0], buf1.size(), g.tell()));
    }

    {
        FileDescriptor h(p, FDMode::Read, CreateIfNecessary::F);
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

TEST_F(FileDescriptorTest, seek1)
{
    fs::path p = directory_ / "testfile";
    EXPECT_THROW(FileDescriptor f(p, FDMode::Read),
                 FileDescriptorException);
    FileUtils::touch(p);
    FileDescriptor f(p, FDMode::Read);
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

TEST_F(FileDescriptorTest, locking)
{
    const fs::path p(directory_ / "lockfile");
    FileDescriptor f(p,
                     FDMode::Write,
                     CreateIfNecessary::T);

    std::unique_lock<decltype(f)> u(f);
    ASSERT_TRUE(static_cast<bool>(u));

    FileDescriptor g(p,
                     FDMode::Write);

    std::unique_lock<decltype(g)> v(g,
                                    std::try_to_lock);
    ASSERT_FALSE(static_cast<bool>(v));
}

TEST_F(FileDescriptorTest, purge_from_page_cache)
{
    const fs::path p(directory_ / "some_file");
    FileDescriptor f(p,
                     FDMode::ReadWrite,
                     CreateIfNecessary::T);

    const size_t page_size = ::sysconf(_SC_PAGESIZE);
    ASSERT_LT(0,
              page_size);

    const std::vector<uint8_t> buf(page_size,
                                   'Q');

    ASSERT_EQ(page_size,
              f.write(buf.data(),
                      buf.size()));

    void* m = ::mmap(0,
                     page_size,
                     PROT_READ,
                     MAP_PRIVATE,
                     get_fd(f),
                     0);
    ASSERT_TRUE(m != MAP_FAILED);

    auto on_exit(make_scope_exit([&]
                                 {
                                     ::munmap(m,
                                              page_size);
                                 }));

    auto check([&]() -> bool
               {
                   uint8_t chk = 0;
                   EXPECT_EQ(0,
                             ::mincore(m,
                                       page_size,
                                       &chk));
                   return chk != 0;
               });

    EXPECT_TRUE(check());

    f.sync();
    f.fadvise(FAdvise::DontNeed);

    EXPECT_FALSE(check());
}

}

// Local Variables: **
// mode: c++ **
// End: **
