// Copyright 2015 iNuron NV
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

#include "../BackendConnectionInterface.h"
#include "../SimpleFetcher.h"

#include <youtils/SourceOfUncertainty.h>
#include <youtils/FileUtils.h>
#include <youtils/FileDescriptor.h>

namespace backend
{

namespace fs = boost::filesystem;
using namespace youtils;

class PartialReadTest
    : public BackendTestBase
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

    void
    TearDown() override final
    {}

protected:
    const fs::path partial_read_cache_;
    std::unique_ptr<WithRandomNamespace> nspace_;
};

TEST_F(PartialReadTest, simple)
{
    using PartialReads = backend::BackendConnectionInterface::PartialReads;

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

    PartialReads partial_reads;

    while(last < max_test_size)
    {
        partial_reads.emplace_back(std::string(object_name));
        auto& partial_read = partial_reads.back();
        partial_read.offset = last;
        uint64_t size = sou(0UL, max_test_size - last);
        partial_read.buf = the_buffer.data() + last;
        partial_read.size = size;
        last += size;
        ASSERT(last <= max_test_size);
    }

    BackendConnectionInterfacePtr conn(cm_->getConnection());
    SimpleFetcher fetch(*conn,
                        nspace_->ns(),
                        partial_read_cache_);

    bi_(nspace_->ns())->partial_read(partial_reads,
                                     fetch,
                                     InsistOnLatestVersion::T);

    for(const auto& partial_read : partial_reads)
    {
        for(unsigned i = 0; i < partial_read.size; ++i)
        {
            EXPECT_EQ(the_buffer[partial_read.offset + i],
                      buf[partial_read.offset +i]);
        }
    }
}

TEST_F(PartialReadTest, simple_to)
{
    using PartialReads = backend::BackendConnectionInterface::PartialReads;

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

    PartialReads partial_reads;

    while(last < max_test_size)
    {
        partial_reads.emplace_back(std::string(object_name));
        auto& partial_read = partial_reads.back();
        partial_read.offset = last;
        uint64_t size = sou(0UL, max_test_size - last);
        partial_read.buf = the_buffer.data() + last;
        partial_read.size = size;

        last += size;
        last += sou(0UL, max_test_size - last);
        ASSERT(last <= max_test_size);
    }

    BackendConnectionInterfacePtr conn(cm_->getConnection());
    SimpleFetcher fetch(*conn,
                        nspace_->ns(),
                        partial_read_cache_);

    bi_(nspace_->ns())->partial_read(partial_reads,
                                     fetch,
                                     InsistOnLatestVersion::T);

    for(const auto& partial_read : partial_reads)
    {
        for(unsigned i = 0; i < partial_read.size; ++i)
        {
            EXPECT_EQ(the_buffer[partial_read.offset + i],
                      buf[partial_read.offset +i]);
        }
    }
}

}
