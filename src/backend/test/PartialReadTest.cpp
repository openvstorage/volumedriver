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


#include "BackendTestBase.h"

#include "../BackendConnectionInterface.h"
#include "../SimpleFetcher.h"

#include <boost/thread.hpp>

#include <gtest/gtest.h>

#include <youtils/SourceOfUncertainty.h>
#include <youtils/FileUtils.h>
#include <youtils/FileDescriptor.h>
#include <youtils/System.h>
#include <youtils/Timer.h>

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
                           const std::vector<uint8_t>& src,
                           const std::vector<uint8_t>& dst)
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
    DECLARE_LOGGER("PartialReadTest");

    const fs::path partial_read_cache_;
    std::unique_ptr<WithRandomNamespace> nspace_;
};

TEST_P(PartialReadTest, simple)
{
    const uint64_t max_test_size = 1024 * 16;

    std::vector<uint8_t> buf(max_test_size);
    SourceOfUncertainty sou;

    for(unsigned i = 0; i < max_test_size; ++i)
    {
        buf[i] = sou.operator()<uint8_t>();
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
    std::vector<uint8_t> the_buffer(max_test_size);

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

    std::vector<uint8_t> buf(max_test_size);
    SourceOfUncertainty sou;

    for(unsigned i = 0; i < max_test_size; ++i)
    {
        buf[i] = sou.operator()<uint8_t>();
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
    std::vector<uint8_t> the_buffer(max_test_size);

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

    std::vector<uint8_t> buf(max_test_size);
    SourceOfUncertainty sou;

    auto make_object_name([](const size_t n) -> std::string
                          {
                              return "object-" + boost::lexical_cast<std::string>(n);
                          });

    for(unsigned i = 0; i < max_test_size; ++i)
    {
        buf[i] = sou.operator()<uint8_t>();
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

    std::vector<uint8_t> the_buffer(max_test_size);

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

TEST_P(PartialReadTest, DISABLED_performance)
{
    const size_t nnspaces = yt::System::get_env_with_default("BACKEND_PARTIAL_READ_TEST_NAMESPACES",
                                                             1);

    ASSERT_NE(0,
              nnspaces) << "fix your test";

    const size_t nthreads = yt::System::get_env_with_default("BACKEND_PARTIAL_READ_TEST_THREADS",
                                                             1);

    ASSERT_NE(0,
              nthreads);
    EXPECT_LE(nnspaces,
              nthreads) << "having more namespaces than threads does not make sense for this test";

    const size_t nobjs = yt::System::get_env_with_default("BACKEND_PARTIAL_READ_TEST_OBJECT_COUNT",
                                                          1);
    ASSERT_NE(0,
              nobjs);

    const size_t obj_size = yt::System::get_env_with_default("BACKEND_PARTIAL_READ_TEST_OBJECT_SIZE",
                                                            4ULL << 20);
    ASSERT_NE(0,
              obj_size);

    const size_t slice_size = yt::System::get_env_with_default("BACKEND_PARTIAL_READ_TEST_SLICE_SIZE",
                                                               4ULL << 10);
    ASSERT_NE(0,
              slice_size);
    ASSERT_EQ(0,
              obj_size % slice_size);

    const size_t nslices = yt::System::get_env_with_default("BACKEND_PARTIAL_READ_TEST_SLICE_COUNT",
                                                            1);

    const size_t iterations = yt::System::get_env_with_default("BACKEND_PARTIAL_READ_TEST_ITERATIONS",
                                                               1ULL << 10);
    ASSERT_NE(0,
              iterations);

    std::vector<std::unique_ptr<WithRandomNamespace>> nspaces;
    nspaces.reserve(nnspaces);

    for (size_t i = 0; i < nnspaces; ++i)
    {
        nspaces.emplace_back(make_random_namespace());
    }

    {
        const fs::path path(yt::FileUtils::create_temp_file_in_temp_dir("some-object"));
        ALWAYS_CLEANUP_FILE(path);

        {
            const std::vector<uint8_t> buf(obj_size);

            FileDescriptor io(path,
                              FDMode::Write);
            io.write(buf.data(),
                     buf.size());
        }

        std::vector<boost::thread> threads;
        threads.reserve(nspaces.size());

        auto upload([&](const Namespace& nspace)
                    {
                        BackendInterfacePtr bi(bi_(nspace));

                        for (size_t i = 0; i < nobjs; ++i)
                        {
                            bi->write(path,
                                      boost::lexical_cast<std::string>(i),
                                      OverwriteObject::F);
                        }
                    });

        yt::SteadyTimer t;
        for (size_t i = 0; i < nspaces.size(); ++i)
        {
            threads.emplace_back(boost::thread([&, i]
                                               {
                                                   upload(nspaces[i]->ns());
                                               }));
        }

        for (auto& t : threads)
        {
            t.join();
        }

        const auto elapsed = t.elapsed();

        std::cout << "Upload of " << nobjs << " objects per namespace of size " << obj_size << " to " <<
            nspaces.size() << " namespaces took " << elapsed << std::endl;
    }

    auto partial_read([&](SourceOfUncertainty& sou,
                          std::vector<uint8_t>& buf,
                          SimpleFetcher& fetch,
                          BackendInterfacePtr& bi)
                      {
                          ObjectSlices slices;

                          off_t off = sou(0UL,
                                          obj_size / slice_size - 1) * slice_size;

                          for (size_t i = 0; i < nslices; ++i)
                          {
                              ASSERT_TRUE(slices.emplace(slice_size,
                                                         (off + i * slice_size) % obj_size,
                                                         buf.data() + i * slice_size).second);
                          }

                          const std::string oname(boost::lexical_cast<std::string>(sou(0UL,
                                                                                       nobjs - 1)));
                          const PartialReads partial_reads{{oname, std::move(slices)}};

                          bi->partial_read(partial_reads,
                                           fetch,
                                           GetParam());
                      });

    auto work([&](const size_t id)
              {
                  const Namespace& nspace(nspaces[id % nspaces.size()]->ns());
                  BackendInterfacePtr bi(bi_(nspace));
                  SourceOfUncertainty sou;
                  BackendConnectionInterfacePtr conn(cm_->getConnection());
                  SimpleFetcher fetch(*conn,
                                      nspace_->ns(),
                                      partial_read_cache_);

                  std::vector<uint8_t> buf(nslices * slice_size);

                  for (size_t i = 0; i < iterations; ++i)
                  {
                      partial_read(sou,
                                   buf,
                                   fetch,
                                   bi);
                  }
              });

    std::vector<boost::thread> threads;
    threads.reserve(nthreads);

    yt::SteadyTimer t;
    for (size_t i = 0; i < nthreads; ++i)
    {
        threads.emplace_back(boost::thread([&, i]
                                           {
                                               work(i);
                                           }));
    }

    for (auto& t : threads)
    {
        t.join();
    }

    const auto elapsed = t.elapsed();
    double secs(boost::chrono::duration_cast<boost::chrono::nanoseconds>(elapsed).count() / 1e9);
    ASSERT_NE(0,
              secs);

    std::cout << nthreads << " threads doing " << iterations << " partial reads of " <<
        nslices << " slices of size " << slice_size << " took " <<  elapsed <<
        " (" << secs << " secs) -> " << (nthreads * iterations / secs) << " IOPS" <<
        std::endl;
}

INSTANTIATE_TEST_CASE_P(PartialReadTests,
                        PartialReadTest,
                        testing::Values(InsistOnLatestVersion::T,
                                        InsistOnLatestVersion::F));

}
