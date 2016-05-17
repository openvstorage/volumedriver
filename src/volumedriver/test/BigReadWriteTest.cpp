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

#include "VolManagerTestSetup.h"

namespace volumedriver
{
class BigReadWriteTest
    : public VolManagerTestSetup
{
public:
    BigReadWriteTest()
        : VolManagerTestSetup("BigReadWriteTest")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }
};

TEST_P(BigReadWriteTest, bigReadsOnEmpty)
{
    auto ns_ptr = make_random_namespace();

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    size_t csz = v->getClusterSize();
    const std::string pattern(csz,'\0');
    size_t scoMul = v->getSCOMultiplier();

    for(size_t i = 0; i < scoMul; ++i)
    {
        checkVolume(*v,0, csz*scoMul, pattern);
    }
}

TEST_P(BigReadWriteTest, bigReadsOnFull)
{
    auto ns_ptr = make_random_namespace();
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    SCOPED_BLOCK_BACKEND(*v);

    size_t csz = v->getClusterSize();
    size_t lba_size = v->getLBASize();

    const std::string pattern(csz,'a');
    size_t scoMul = v->getSCOMultiplier();
    for(size_t i = 0;i < 50*scoMul; ++i)
    {
        writeToVolume(*v, i* csz / lba_size, csz, pattern);
    }

    // Stop here to manually delete sco's to check error handling
    for(size_t i = 0; i < scoMul; ++i)
    {
        checkVolume(*v,0, csz*scoMul, pattern);
    }

}

TEST_P(BigReadWriteTest, bigWritesBigReads)
{
    auto ns_ptr = make_random_namespace();
    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    SCOPED_BLOCK_BACKEND(*v);

    size_t csz = v->getClusterSize();

    const std::string pattern(csz, 'a');
    size_t scoMul = v->getSCOMultiplier();
    for (size_t i = 0; i < scoMul; ++i)
    {
        writeToVolume(*v, 0, csz * (i + 1), pattern);
    }

    // Stop here to manually delete sco's to check error handling
    for (size_t i = 0; i < scoMul; ++i)
    {
        checkVolume(*v, 0, csz * (i + 1), pattern);
    }

}

TEST_P(BigReadWriteTest, OneBigWriteOneBigRead)
{
    auto ns_ptr = make_random_namespace();

    SharedVolumePtr v = newVolume(VolumeId("volume1"),
                                  ns_ptr->ns());

    SCOPED_BLOCK_BACKEND(*v);

    size_t csz = v->getClusterSize();

    const std::string pattern(csz,'a');
    size_t scoMul = v->getSCOMultiplier();
    writeToVolume(*v, 0, csz * scoMul, pattern);

    // Stop here to manually delete sco's to check error handling
    checkVolume(*v,0, csz*scoMul, pattern);

}

INSTANTIATE_TEST(BigReadWriteTest);

}

// Local Variables: **
// mode: c++ **
// End: **
