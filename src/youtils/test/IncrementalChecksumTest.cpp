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
#include <boost/filesystem/fstream.hpp>
#include "../DimensionedValue.h"
#include "../FileDescriptor.h"
#include "../IncrementalChecksum.h"

namespace youtilstest
{
using namespace youtils;

class IncrementalChecksumTest : public TestBase
{};

TEST_F(IncrementalChecksumTest, test0)
{}



TEST_F(IncrementalChecksumTest, test1)
{

    const uint64_t test_size = DimensionedValue("1MiB").getBytes();

    const std::vector<uint8_t> buf(test_size);

    fs::path tmp_file = FileUtils::create_temp_file_in_temp_dir("incrementalChecksumTest");
    ALWAYS_CLEANUP_FILE(tmp_file);
    {
        FileDescriptor io(tmp_file, FDMode::Write);
        io.write(&buf[0], test_size);

    }

    IncrementalChecksum ics(tmp_file);
    off_t offset = 0;


    while((unsigned)offset < test_size)
    {
        CheckSum cs;
        cs.update(&buf[0], offset);
        ASSERT_TRUE(ics.checksum(offset) == cs) << " at offset " << offset;
        double p = drand48();
        if(p > 0.10)
        {
            offset +=  p*4096;
        }

    }
    CheckSum cs;
    cs.update(&buf[0], test_size);

    ASSERT_TRUE(ics.checksum(test_size) == cs);
}

}


// Local Variables: **
// mode: c++ **
// End: **
