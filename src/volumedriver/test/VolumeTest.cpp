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

#include "VolumeDriverTestConfig.h"
#include "VolManagerTestSetup.h"

#include <cstring>
#include <fstream>

#include <boost/optional.hpp>

#include <youtils/Logging.h>

#include <backend/BackendInterface.h>

#include "../TransientException.h"
#include "../VolManager.h"
#include "../Volume.h"

namespace volumedriver
{

class VolumeTest
    : public VolManagerTestSetup
{

protected:
    VolumeTest()
        : VolManagerTestSetup("VolumeTest")
        , vol_(nullptr)
    {}

    void
    SetUp()
    {
        VolManagerTestSetup::SetUp();
        ns_ptr_ = make_random_namespace();

        vol_ = newVolume("volume1",
			 ns_ptr_->ns());
        ASSERT_TRUE(vol_ != nullptr);

        config_ = vol_->get_config();
    }

    void
    TearDown()
    {
        vol_ = nullptr;
        ns_ptr_.reset();
        VolManagerTestSetup::TearDown();
    }

    void
    writeLBAs(Lba lba,
              uint64_t count,
              uint32_t pattern,
              SharedVolumePtr vol = nullptr,
              int retries = 7)
    {
        bool fail = true;
        if (vol == 0) {
            vol = vol_;
        }

        uint64_t bufsz = count * vol->getLBASize();
        std::vector<uint8_t> bufv(bufsz);
        uint8_t *buf = &bufv[0];
        uint64_t off;
        uint32_t *p;

        ASSERT_GE((uint32_t)vol->getClusterSize(), vol->getLBASize());

        for (p = (uint32_t *) buf, off = 0; off < bufsz;
             off += sizeof(*p), ++p)
            *p = pattern;

        do {
            try {
                vol->write(lba, buf, bufsz);
                fail = false;
            } catch (TransientException &e) {
                LOG_INFO("backend congestion detected, retrying");
                sleep(1);
                continue;
            }
            break;
        } while (--retries >= 0);
        if (fail) {
            throw fungi::IOException("Still can't write after 7 tries; giving up");
        }
    }

    void readLBAs(Lba lba,
                  uint64_t count,
                  uint32_t pattern,
                  bool verify=true,
                  SharedVolumePtr vol = nullptr,
                  int retries = 7)
    {
        bool fail = true;
        if (vol == 0) {
            //			LOG_INFO("fallback on vol_(vol== " << vol << ")");
            vol = vol_;
        }

        uint64_t bufsz = count * vol->getLBASize();
        std::vector<uint8_t> bufv(bufsz);
        uint8_t *buf = &bufv[0];
        uint64_t off;
        uint32_t *p;

        do {
            try {
                vol->read(lba, buf, bufsz);
                fail = false;
            } catch (TransientException &e) {
                LOG_INFO("backend congestion detected, retrying");
                sleep(1);
                continue;
            }
            break;
        } while (--retries >= 0);
        if (fail) {
            throw fungi::IOException("Still can't read after 7 tries; giving up");
        }
        if (verify) {
            for (p = (uint32_t *) buf, off = 0; off < bufsz;
                 off += sizeof(*p), ++p)
                ASSERT_EQ(pattern, *p) << "p: " << p <<
                    " off: " << off;
        }
    }

    void
    testClusteredIO(size_t w1_lbas_per_io,
                    size_t w2_lbas_per_io,
                    size_t iterations)
    {
        size_t lbasize = vol_->getLBASize();

        const std::vector<byte> w1_buf(w1_lbas_per_io * lbasize, 'a');
        const std::vector<byte> w2_buf(w2_lbas_per_io * lbasize, 'b');

        // memset(&w1_buf[0], 'a', w1_buf.size());
        // memset(&w2_buf[0], 'z', w2_buf.size());

        const size_t off1 = 0;
        size_t off2 = w1_lbas_per_io * iterations;

        for (size_t i = 0; i < iterations; ++i)
        {
            vol_->write(Lba(off1 + i * w1_lbas_per_io),
                        w1_buf.data(),
                        w1_buf.size());

            vol_->write(Lba(off2 + i * w2_lbas_per_io),
                        w2_buf.data(),
                        w2_buf.size());
        }

        {
            std::vector<byte> rbuf(w1_buf.size() * iterations);
            vol_->read(Lba(off1), &rbuf[0], rbuf.size());
            for (size_t i = 0; i < iterations; ++i)
            {
                EXPECT_EQ(0, memcmp(&rbuf[i * w1_buf.size()],
                                    &w1_buf[0],
                                    w1_buf.size()));
            }
        }

        {
            std::vector<byte> rbuf(w2_buf.size() * iterations);
            vol_->read(Lba(off2), &rbuf[0], rbuf.size());
            for (size_t i = 0; i < iterations; ++i)
            {
                EXPECT_EQ(0, memcmp(&rbuf[i * w2_buf.size()],
                                    &w2_buf[0],
                                    w2_buf.size()));
            }
        }
    }

    DECLARE_LOGGER("TestVolume");

    SharedVolumePtr vol_;
    std::unique_ptr<WithRandomNamespace> ns_ptr_;
    boost::optional<VolumeConfig> config_;
};

TEST_P(VolumeTest, VolumeConfig)
{
    ASSERT_EQ(vol_->getSize(), vol_->getLBASize() * vol_->getLBACount());
}

TEST_P(VolumeTest, readCluster)
{
    ASSERT_NO_THROW(readLBAs(Lba(0), vol_->getClusterSize() / vol_->getLBASize(),
                             0, false));
}

TEST_P(VolumeTest, readUnderflow)
{
    uint64_t count = vol_->getClusterSize() / vol_->getLBASize() - 1;
    ASSERT_NO_THROW(readLBAs(Lba(0), count, 0, false));
}

TEST_P(VolumeTest, readOverflow)
{
    uint64_t count = vol_->getClusterSize() / vol_->getLBASize() + 1;
    ASSERT_NO_THROW(readLBAs(Lba(0), count, 0, false));
}

TEST_P(VolumeTest, writeFirstLBA)
{
    uint32_t pattern = 0xdeadbeef;
    ASSERT_NO_THROW(writeLBAs(Lba(0), 1, pattern));
    ASSERT_NO_THROW(readLBAs(Lba(0), 1, pattern));
}

TEST_P(VolumeTest, writeUnderflow)
{
    uint64_t count = vol_->getClusterSize() / vol_->getLBASize() - 1;
    uint32_t pattern = 0xdeadbeef;
    ASSERT_NO_THROW(writeLBAs(Lba(0), count, pattern));
    ASSERT_NO_THROW(readLBAs(Lba(0), count, pattern));
}

TEST_P(VolumeTest, writeOverflow)
{
    uint64_t count = vol_->getClusterSize() / vol_->getLBASize() + 1;
    uint32_t pattern = 0xdeadf00d;
    ASSERT_NO_THROW(writeLBAs(Lba(0), count, pattern));
    ASSERT_NO_THROW(readLBAs(Lba(0), count, pattern));
}

TEST_P(VolumeTest, writeUnalignedLBA)
{
    uint64_t count = vol_->getClusterSize() / vol_->getLBASize() - 1;
    uint32_t pattern = 0x01234567;
    ASSERT_NO_THROW(writeLBAs(Lba(count - 1), count, pattern));
    ASSERT_NO_THROW(readLBAs(Lba(count - 1), count, pattern));
}

TEST_P(VolumeTest, reWriteUnalignedLBAs)
{
    const uint64_t count1 = config_->cluster_mult_ * config_->sco_mult_ * 3;
    const uint32_t pattern1 = 0x01234567;
    ASSERT_NO_THROW(writeLBAs(Lba(0), count1, pattern1));
    ASSERT_NO_THROW(readLBAs(Lba(0), count1, pattern1));

    const uint64_t count2 = config_->cluster_mult_;
    const uint32_t pattern2 = 0xcafebeef;
    ASSERT_NO_THROW(writeLBAs(Lba(1), count2, pattern2));
    ASSERT_NO_THROW(readLBAs(Lba(1), count2, pattern2));
    ASSERT_NO_THROW(readLBAs(Lba(0), 1, pattern1));
}

TEST_P(VolumeTest, writeUnalignedLen)
{
    uint64_t count = vol_->getClusterSize() / vol_->getLBASize();
    uint32_t pattern = 0x01234567;
    ASSERT_NO_THROW(writeLBAs(Lba(count), count + 1, pattern));
    ASSERT_NO_THROW(readLBAs(Lba(count), count + 1, pattern));
}

TEST_P(VolumeTest, writeUnalignedLenAndLBA)
{
    uint64_t count = (vol_->getClusterSize() / vol_->getLBASize()) * 3 - 1;
    const Lba lba(vol_->getClusterSize() / vol_->getLBASize() - 1);
    uint32_t pattern = 0x01234567;
    ASSERT_NO_THROW(writeLBAs(lba, count, pattern));
    ASSERT_NO_THROW(readLBAs(lba, count, pattern));
}

TEST_P(VolumeTest, writeBeyondEnd)
{
    const Lba lba(vol_->getSize() / vol_->getLBASize() +
                  vol_->getClusterSize() / vol_->getLBASize());
    ASSERT_THROW(writeLBAs(lba, vol_->getClusterSize() / vol_->getLBASize(),
                           0x89ABCDEF), fungi::IOException) <<
        "VolumeSize: " << vol_->getSize() << " LBASize: " <<
        vol_->getLBASize() << " LBA: " << lba << std::endl;
}

TEST_P(VolumeTest, writeFirstCluster)
{
    uint32_t pattern = 0xdeadbeef;
    ASSERT_NO_THROW(writeLBAs(Lba(0),
                              vol_->getClusterSize() / vol_->getLBASize(),
                              pattern));
    ASSERT_NO_THROW(readLBAs(Lba(0),
                             vol_->getClusterSize() / vol_->getLBASize(),
                             pattern));
}

TEST_P(VolumeTest, writeThirdCluster)
{
    uint64_t lba = vol_->getClusterSize() * 3 / vol_->getLBASize();
    uint32_t pattern = 0xdeadbeef;
    ASSERT_NO_THROW(writeLBAs(Lba(0), lba, pattern));
    ASSERT_NO_THROW(readLBAs(Lba(0), lba, pattern));
}

TEST_P(VolumeTest, writeClusters)
{
    uint32_t pattern = 0xdeadf00d;
    int count = 2;
    uint64_t lbacnt = (count * vol_->getClusterSize()) / vol_->getLBASize();
    ASSERT_NO_THROW(writeLBAs(Lba(0), lbacnt, pattern));
    ASSERT_NO_THROW(readLBAs(Lba(0), lbacnt, pattern));
}

TEST_P(VolumeTest, writeClustersAcrossSCOs)
{
    int count = config_->sco_mult_ * config_->cluster_mult_ + 13;
    uint32_t pattern = 0xfeedbeef;
    ASSERT_NO_THROW(writeLBAs(Lba(0), count, pattern));
    ASSERT_NO_THROW(readLBAs(Lba(0), count, pattern));
}

TODO("Y42: HOW IS THIS POSSIBLE -> 2 tests have the same body")

TEST_P(VolumeTest, DISABLED_clusteredIOTestNoClusterCache)
{

    size_t max_lbas = vol_->getClusterSize() *
        (vol_->getClusterMultiplier() + 1) /
        vol_->getLBASize();

    for (size_t i = 1; i < max_lbas; ++i)
    {
        for (size_t j = 1; j < max_lbas; ++j)
        {
            testClusteredIO(i, j, 2 * max_lbas / (i + j));
        }
    }
}

TEST_P(VolumeTest, DISABLED_clusteredIOTestWithClusterCache)
{

    size_t max_lbas = vol_->getClusterSize() *
        (vol_->getClusterMultiplier() + 1) /
        vol_->getLBASize();

    for (size_t i = 1; i < max_lbas; ++i)
    {
        for (size_t j = 1; j < max_lbas; ++j)
        {
            testClusteredIO(i, j, 2 * max_lbas / (i + j));
        }
    }
}

INSTANTIATE_TEST(VolumeTest);

}

// Local Variables: **
// mode: c++ **
// End: **
