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

#include <boost/iostreams/stream.hpp>

#include "../FileRange.h"
#include "../FileDescriptor.h"

namespace youtilstest
{
using namespace youtils;

class FileRangeTest
    : public TestBase
{
public:
    FileRangeTest()
        : dir_(getTempPath("FileRangeTest"))
    {}

    virtual void
    SetUp()
    {
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        const fs::path p(dir_ / "testfile");
        test_file_.reset(new file_type(p, FDMode::ReadWrite, CreateIfNecessary::T));
        test_file_->fallocate(1ULL << 20);
    }

    virtual void
    TearDown()
    {
        fs::remove_all(dir_);
    }

    using file_type = FileDescriptor;

protected:

    const fs::path dir_;
    std::shared_ptr<file_type> test_file_;
};

typedef FileRange<FileRangeTest::file_type> file_range_type;

TEST_F(FileRangeTest, construction)
{
    const uint64_t size = test_file_->size();
    EXPECT_LT(0, size);

    EXPECT_THROW(file_range_type(test_file_,
                                 0,
                                 size + 1),
                 FileRangeViolation);

    EXPECT_THROW(file_range_type(test_file_,
                                 size,
                                 1),
                 FileRangeViolation);

    EXPECT_LT(0, size / 3);
    file_range_type r(test_file_,
                      size / 3,
                      size / 3);
}

TEST_F(FileRangeTest, basics)
{
    const size_t size = test_file_->size();
    EXPECT_LT(0, size);

    const uint8_t segments = 17;
    const ssize_t seg_size = size / segments;
    EXPECT_LT(0, seg_size);

    for (uint8_t i = 0; i < segments; ++ i)
    {
        const std::vector<uint8_t> b(seg_size, i);
        file_range_type r(test_file_,
                          i * seg_size,
                          seg_size);
        EXPECT_EQ(seg_size, r.pwrite(&b[0],
                                     b.size(),
                                     0));
    }

    for (uint8_t i = 0; i < segments; ++i)
    {
        const std::vector<uint8_t> ref(seg_size, i);
        std::vector<uint8_t> b(seg_size);

        file_range_type r(test_file_,
                          i * seg_size,
                          seg_size);

        EXPECT_EQ(seg_size, r.pread(&b[0],
                                    b.size(),
                                    0));
        EXPECT_EQ(0, memcmp(&ref[0], &b[0], ref.size()));

        EXPECT_EQ(seg_size, test_file_->pread(&b[0],
                                              b.size(),
                                              seg_size * i));
        EXPECT_EQ(0, memcmp(&ref[0], &b[0], ref.size()));
    }
}

TEST_F(FileRangeTest, boundaries)
{
    const size_t size = test_file_->size();
    EXPECT_LT(0, size);

    const uint8_t segments = 7;
    const ssize_t seg_size = size / segments;
    EXPECT_LT(0, seg_size);

    const std::vector<uint8_t> ref(seg_size, segments);

    for (uint8_t i = 0; i < segments; ++i)
    {
        EXPECT_EQ(ref.size(), test_file_->pwrite(&ref[0],
                                                 ref.size(),
                                                 i * ref.size()));

        file_range_type r(test_file_,
                          seg_size * i,
                          seg_size);

        const std::vector<uint8_t> wbuf(seg_size + 1, i);
        // a bit (byte) too big
        EXPECT_THROW(r.pwrite(&wbuf[0], wbuf.size(), 0),
                     FileRangeViolation);

        // a bit (byte) off
        EXPECT_THROW(r.pwrite(&wbuf[0], seg_size, 1),
                     FileRangeViolation);

        // way off
        EXPECT_THROW(r.pwrite(&wbuf[0], seg_size, seg_size),
                     FileRangeViolation);

        EXPECT_EQ(0, r.pwrite(&wbuf[0], 0, 0));
        EXPECT_EQ(0, r.pwrite(&wbuf[0], 0, seg_size));

        std::vector<uint8_t> rbuf(seg_size);

        EXPECT_EQ(seg_size, r.pread(&rbuf[0], rbuf.size(), 0));
        EXPECT_EQ(0, memcmp(&ref[0], &rbuf[0], ref.size()));

        EXPECT_EQ(seg_size, test_file_->pread(&rbuf[0], rbuf.size(), seg_size * i));
        EXPECT_EQ(0, memcmp(&ref[0], &rbuf[0], ref.size()));
    }
}

TEST_F(FileRangeTest, streaming)
{
    const size_t size = test_file_->size();
    EXPECT_LT(0, size);

    const uint8_t segments = 2;
    const ssize_t seg_size = size / segments;

    EXPECT_LT(0, seg_size);

    const size_t iosize = 4096;
    EXPECT_EQ(0, seg_size % iosize);
    EXPECT_LT(iosize, seg_size);

    for (uint8_t i = 0; i < segments; ++i)
    {
        file_range_type r(test_file_,
                          i * seg_size,
                          iosize);

        bio::stream<file_range_type> io(file_range_type(test_file_,
                                                        i * seg_size,
                                                        seg_size),
                                        4096);
        for (size_t j = 0; j < seg_size / iosize; ++j)
        {
            const std::vector<uint8_t> wbuf(4096, i + j);
            io.write(reinterpret_cast<const char*>(&wbuf[0]), wbuf.size());
            EXPECT_TRUE(io.good());
        }
    }

    for (uint8_t i = 0; i < segments; ++i)
    {
        file_range_type r(test_file_,
                          i * seg_size,
                          iosize);

        bio::stream<file_range_type> io(file_range_type(test_file_,
                                                        i * seg_size,
                                                        seg_size),
                                        4096);
        for (size_t j = 0; j < seg_size / iosize; ++j)
        {
            const std::vector<uint8_t> ref(iosize, i + j);
            std::vector<uint8_t> rbuf(iosize);

            io.read(reinterpret_cast<char*>(&rbuf[0]), rbuf.size());
            EXPECT_TRUE(io.good());
            EXPECT_EQ(0, memcmp(&ref[0], &rbuf[0], ref.size()));
        }
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
