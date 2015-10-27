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

    Volume* v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    size_t csz = v->getClusterSize();
    const std::string pattern(csz,'\0');
    size_t scoMul = v->getSCOMultiplier();

    for(size_t i = 0; i < scoMul; ++i)
    {
        checkVolume(v,0, csz*scoMul, pattern);
    }
}

TEST_P(BigReadWriteTest, bigReadsOnFull)
{
    auto ns_ptr = make_random_namespace();
    Volume* v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    SCOPED_BLOCK_BACKEND(v);

    size_t csz = v->getClusterSize();
    size_t lba_size = v->getLBASize();

    const std::string pattern(csz,'a');
    size_t scoMul = v->getSCOMultiplier();
    for(size_t i = 0;i < 50*scoMul; ++i)
    {
        writeToVolume(v, i* csz / lba_size, csz, pattern);
    }

    // Stop here to manually delete sco's to check error handling
    for(size_t i = 0; i < scoMul; ++i)
    {
        checkVolume(v,0, csz*scoMul, pattern);
    }

}

TEST_P(BigReadWriteTest, bigWritesBigReads)
{
    auto ns_ptr = make_random_namespace();
    Volume* v = newVolume(VolumeId("volume1"),
                          ns_ptr->ns());

    SCOPED_BLOCK_BACKEND(v);

    size_t csz = v->getClusterSize();

    const std::string pattern(csz, 'a');
    size_t scoMul = v->getSCOMultiplier();
    for (size_t i = 0; i < scoMul; ++i)
    {
        writeToVolume(v, 0, csz * (i + 1), pattern);
    }

    // Stop here to manually delete sco's to check error handling
    for (size_t i = 0; i < scoMul; ++i)
    {
        checkVolume(v, 0, csz * (i + 1), pattern);
    }

}

TEST_P(BigReadWriteTest, OneBigWriteOneBigRead)
{
    auto ns_ptr = make_random_namespace();

    Volume* v = newVolume(VolumeId("volume1"),
			  ns_ptr->ns());

    SCOPED_BLOCK_BACKEND(v);

    size_t csz = v->getClusterSize();

    const std::string pattern(csz,'a');
    size_t scoMul = v->getSCOMultiplier();
    writeToVolume(v, 0, csz * scoMul, pattern);

    // Stop here to manually delete sco's to check error handling
    checkVolume(v,0, csz*scoMul, pattern);

}

INSTANTIATE_TEST(BigReadWriteTest);

}

// Local Variables: **
// mode: c++ **
// End: **
