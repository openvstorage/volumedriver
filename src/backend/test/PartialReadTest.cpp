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

#include <gtest/gtest.h>

#include "BackendTestBase.h"

#include "../BackendConnectionInterface.h"
#include "../SimpleFetcher.h"

#include <youtils/SourceOfUncertainty.h>
#include <youtils/FileUtils.h>
#include <youtils/FileDescriptor.h>

namespace backendtest
{

namespace fs = boost::filesystem;

using namespace youtils;
using namespace backend;

class PartialReadTest
    : public BackendTestBase
    , public testing::WithParamInterface<InsistOnLatestVersion>
{
public:
    PartialReadTest()
        : BackendTestBase("PartialReadTest")
        , partial_read_cache_(path_ / "partial-read-cache")
    {}

    void
    SetUp() override final
    {
BackendTestBase::SetUp();
        fs::create_directories(partial_read_cache_);

        nspace_ = make_random_namespace();
    }

    using PartialReads = BackendConnectionInterface::PartialReads;
    using ObjectSlices = BackendConnectionInterface::ObjectSlices;

    void
    partial_read_and_check(const PartialReads& partial_reads,
                           const std::vector<byte>& src,
                           const std::vector<byte>& dst)
    {
        BackendConnectionInterfacePtr conn(cm_->getConnection());
        SimpleFetcher fetch(*conn,
                            nspace_->ns(),
                            partial_read_cache_);

        bi_(nspace_->ns())->partial_read(partial_reads,
                                         fetch,
                                         GetParam());

        for(const auto& partial_read : partial_reads)
        {
            for (const auto& slice : partial_read.second)
            {
                for(unsigned i = 0; i < slice.size; ++i)
                {
                    EXPECT_EQ(src[slice.offset + i],
                              dst[slice.offset +i]);
                }
            }
        }
    }

protected:
    const fs::path partial_read_cache_;
    std::unique_ptr<WithRandomNamespace> nspace_;
};

TEST_P(PartialReadTest, simple)
{
    const uint64_t max_test_size = 1024 * 16;

    std::vector<byte> buf(max_test_size);
    SourceOfUncertainty sou;

    for(unsigned i = 0; i < max_test_size; ++i)
    {
        buf[i] = sou.operator()<byte>();
    }

    fs::path tmp = FileUtils::create_temp_file_in_temp_dir("anobject");
    ALWAYS_CLEANUP_FILE(tmp);
    {
        FileDescriptor io(tmp,
                          FDMode::Write);
        io.write(buf.data(),
                 max_test_size);
    }

    const std::string object_name("object_name");

    bi_(nspace_->ns())->write(tmp,
                              object_name,
                              OverwriteObject::F);

    uint64_t last = 0;
    std::vector<byte> the_buffer(max_test_size);

    ObjectSlices slices;

    while(last < max_test_size)
    {
        const uint64_t off = last;
        const uint64_t size = sou(1UL, max_test_size - last);

        ASSERT_TRUE(slices.emplace(size,
                                   off,
                                   the_buffer.data() + last).second);
        last += size;
        ASSERT(last <= max_test_size);
    }

    const PartialReads partial_reads{ { object_name, std::move(slices) } };

    partial_read_and_check(partial_reads,
                           buf,
                           the_buffer);
}

TEST_P(PartialReadTest, simple_too)
{
    const uint64_t max_test_size = 1024 * 16;

    std::vector<byte> buf(max_test_size);
    SourceOfUncertainty sou;

    for(unsigned i = 0; i < max_test_size; ++i)
    {
        buf[i] = sou.operator()<byte>();
    }

    fs::path tmp = FileUtils::create_temp_file_in_temp_dir("anobject");
    ALWAYS_CLEANUP_FILE(tmp);
    {

        FileDescriptor io(tmp,
                          FDMode::Write);
        io.write(buf.data(),
                 max_test_size);
    }

    const std::string object_name("object_name");

    bi_(nspace_->ns())->write(tmp,
                              object_name,
                              OverwriteObject::F);

    uint64_t last = 0;
    std::vector<byte> the_buffer(max_test_size);

    ObjectSlices slices;

    while(last < max_test_size)
    {
        const uint64_t off = last;
        const uint64_t size = sou(1UL, max_test_size - last);

        ASSERT_TRUE(slices.emplace(size,
                                   off,
                                   the_buffer.data() + last).second);
        last += size;
        ASSERT(last <= max_test_size);
    }

    const PartialReads partial_reads{ { object_name, std::move(slices) } };

    partial_read_and_check(partial_reads,
                           buf,
                           the_buffer);
}

TEST_P(PartialReadTest, multiple_objects)
{
    const size_t nfiles = 16;
    const size_t fsize = 1024;
    const uint64_t max_test_size = fsize * nfiles;

    std::vector<byte> buf(max_test_size);
    SourceOfUncertainty sou;

    auto make_object_name([](const size_t n) -> std::string
                          {
                              return "object-" + boost::lexical_cast<std::string>(n);
                          });

    for(unsigned i = 0; i < max_test_size; ++i)
    {
        buf[i] = sou.operator()<byte>();
    }

    for (size_t i = 0; i < nfiles; ++i)
    {
        const fs::path tmp(FileUtils::create_temp_file_in_temp_dir("anobject"));
        ALWAYS_CLEANUP_FILE(tmp);
        {
            FileDescriptor io(tmp,
                              FDMode::Write);
            io.write(buf.data() + i * fsize,
                     fsize);
        }

        bi_(nspace_->ns())->write(tmp,
                                  make_object_name(i),
                                  OverwriteObject::F);
    }

    std::vector<byte> the_buffer(max_test_size);

    const size_t nslices = 4;
    const size_t slice_size = fsize / nslices;
    ASSERT_EQ(0, fsize % nslices);
    ASSERT_NE(0, slice_size);

    PartialReads partial_reads;

    for (size_t i = 0; i < nfiles; ++i)
    {
        ObjectSlices slices;

        for (size_t off = 0; off < fsize; off += slice_size)
        {
            ASSERT_TRUE(slices.emplace(slice_size,
                                       off,
                                       the_buffer.data() + i * fsize + off).second);
        }

        ASSERT_TRUE(partial_reads.emplace(std::make_pair(make_object_name(i),
                                                         std::move(slices))).second);
    }

    EXPECT_EQ(nfiles,
              partial_reads.size());

    partial_read_and_check(partial_reads,
                           buf,
                           the_buffer);
}

INSTANTIATE_TEST_CASE_P(PartialReadTests,
                        PartialReadTest,
                        testing::Values(InsistOnLatestVersion::T,
                                        InsistOnLatestVersion::F));

}
