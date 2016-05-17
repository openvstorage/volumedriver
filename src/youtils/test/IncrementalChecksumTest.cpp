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
