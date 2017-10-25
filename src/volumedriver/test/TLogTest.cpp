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

#include "LBAGenerator.h"
#include "VolumeDriverTestConfig.h"
#include "VolManagerTestSetup.h"

#include <math.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/archive/archive_exception.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/timer.hpp>
#include <boost/scope_exit.hpp>

#include <youtils/FileUtils.h>
#include <youtils/System.h>

#include "../CombinedTLogReader.h"
#include "../TLogWriter.h"
#include "../ClusterLocation.h"
#include "../TLogReader.h"
#include "../BackwardTLogReader.h"
#include "../TLog.h"
#include "../Types.h"

namespace volumedrivertest
{

using namespace volumedriver;

namespace fs = boost::filesystem;
namespace yt = youtils;

class TLogTest
    : public testing::TestWithParam<VolumeDriverTestConfig>
{
public:
    TLogTest()
        : directory_(yt::FileUtils::temp_path("TLogTest"))
    {}

    DECLARE_LOGGER("TLogTest");

    virtual void
    SetUp()
    {
        fs::remove_all(directory_);
        fs::create_directories(directory_);
    }

    virtual void
    TearDown()
    {
        fs::remove_all(directory_);
    }

protected:
    const fs::path directory_;
};

TEST_F(TLogTest, first)
{
    TLogs tlogs;
    Serialization::serializeNVPAndFlush<boost::archive::xml_oarchive>(directory_ / "temp_archive",
                                                                      "tlogs",
                                                                      tlogs);
}

TEST_F(TLogTest, second)
{
    TLogs tlogs;

    OrderedTLogIds in;
    std::vector<TLog> out(1);
    out.back().writtenToBackend(true);

    tlogs.replace(in,
                  out);
}

void assertTLogReadersEqual(TLogReaderInterface* x, TLogReaderInterface* y)
{
    const Entry * ex;
    const Entry * ey;
    while ((ex = x->nextLocation())) {
        ey = y->nextLocation();
        ASSERT_TRUE(ey);
        ASSERT_TRUE(*ex == *ey );
    }
    ASSERT_FALSE(y->nextLocation());
}

// Y42 Rest of this should move to TLogWriterTest!!!
TEST_F(TLogTest, combined)
{
    fs::path p1 = yt::FileUtils::temp_path("tmp1");
    fs::remove(p1);
    fs::path p2 = yt::FileUtils::temp_path("tmp2");
    fs::remove(p2);
    fs::path p3 = yt::FileUtils::temp_path("tmp3");
    fs::remove(p3);
    fs::path p123 = yt::FileUtils::temp_path("tmp123");
    fs::remove(p123);

    int total_size = 10;

    try
    {
        for (int size = 0; size < total_size; ++size)
        {
            {
                TLogWriter t1(p1);
                TLogWriter t2(p2);
                TLogWriter t3(p3);
                TLogWriter t123(p123);

                for (int i = 0; i < size; ++i) {
                    ClusterLocation l(i+1, 0, SCOCloneID(1));
                    ClusterLocationAndHash
                        loc_and_hash(l, VolManagerTestSetup::growWeed());
                    t1.add(i, loc_and_hash);
                    t123.add(i, loc_and_hash);
                }

                for (int i = 0; i < size; ++i) {
                    ClusterLocation l(size + i, 0, SCOCloneID(1));
                    ClusterLocationAndHash
                        loc_and_hash(l, VolManagerTestSetup::growWeed());
                    t2.add(size + i, loc_and_hash);
                    t123.add(size + i, loc_and_hash);
                }

                for (int i = 0; i < size; ++i) {
                    ClusterLocation l((size * 2) + i, 0, SCOCloneID(1));
                    ClusterLocationAndHash
                        loc_and_hash(l, VolManagerTestSetup::growWeed());
                    t3.add((size * 2) + i, loc_and_hash);
                    t123.add((size * 2) + i, loc_and_hash);
                }
            }


            std::vector<std::string> paths;
            paths.push_back(p1.string());
            paths.push_back(p2.string());
            paths.push_back(p3.string());

            fs::path empty;

            {
                std::shared_ptr<TLogReaderInterface> reader1 = CombinedTLogReader::create(empty, paths, 0);
                TLogReader reader2(p123);
                assertTLogReadersEqual(reader1.get(), &reader2);
            }

            {
                std::shared_ptr<TLogReaderInterface> reader1 = CombinedTLogReader::create_backward_reader(empty, paths, 0);
                BackwardTLogReader reader2(p123);
                assertTLogReadersEqual(reader1.get(), &reader2);
            }

            fs::remove(p1);
            fs::remove(p2);
            fs::remove(p3);
            fs::remove(p123);
        }
    }
    catch(...)
    {
        fs::remove(p1);
        fs::remove(p2);
        fs::remove(p3);
        fs::remove(p123);
        throw;

    }
}

namespace
{

//[BDV] to be moved to performancetesters
void
timeRead(std::unique_ptr<TLogGen> gen,
         std::string testMsg)
{
    CombinedTLogReader reader(std::move(gen));

    uint64_t items = 0;
    yt::wall_timer wt;

    const Entry* e;
    while((e = reader.nextAny()))
    {
        items++;
    }
    //    timing.stop();
    std::cout << testMsg << ": read " << items << " entries in " <<  wt.elapsed() <<
        " seconds" << std::endl;
}

}

//[BDV] to be moved to performancetesters
TEST_F(TLogTest, prefetchSpeedTest)
{
    using RepeatGenerator = yt::RepeatGenerator<LBAGenGen>;
    using DelayedGenerator = yt::DelayedGenerator<TLogGenItem>;
    using ThreadedGenerator = yt::ThreadedGenerator<TLogGenItem>;

    const uint64_t backend_delay = yt::System::get_env_with_default("BACKEND_DELAY_US",
                                                                    80000ULL);
    const uint32_t tlogs = yt::System::get_env_with_default("NUM_TLOGS",
                                                            32U);
    const uint32_t entries_per_tlog = yt::System::get_env_with_default("TLOG_ENTRIES",
                                                                       1U << 20);
    LBAGenGen defaultg(entries_per_tlog);
    LBAGenGen emptyg(0);

    timeRead(std::make_unique<RepeatGenerator>(defaultg,
                                               tlogs),
             "No backend delay");

    {
        auto rg(std::make_unique<RepeatGenerator>(emptyg,
                                                  tlogs));
        auto dg(std::make_unique<DelayedGenerator>(std::move(rg),
                                                   backend_delay));
        timeRead(std::move(dg),
                 "No processing: only backend");
    }

    {
        auto rg(std::make_unique<RepeatGenerator>(defaultg,
                                                  tlogs));
        auto dg(std::make_unique<DelayedGenerator>(std::move(rg),
                                                   tlogs));
        timeRead(std::move(dg),
                 "Combined");
    }

    {
        auto rg(std::make_unique<RepeatGenerator>(defaultg,
                                                  tlogs));
        auto dg(std::make_unique<DelayedGenerator>(std::move(rg),
                                                   tlogs));
        auto tg(std::make_unique<ThreadedGenerator>(std::move(dg),
                                                    10));
        timeRead(std::move(tg),
                 "With Prefetch");
    }
}

TEST_F(TLogTest, sync)
{
    fs::path p1 = yt::FileUtils::temp_path("tmp1");
    fs::remove(p1);
    ALWAYS_CLEANUP_FILE(p1);
    ClusterLocationAndHash l(ClusterLocation(10), VolManagerTestSetup::growWeed());

    {
        TLogWriter w(p1);
        w.add(0,l);
        w.add();
        w.add(0,l);
        w.add(0,l);
        w.add();
        w.add();
        w.add(0,l);
    }


    const Entry* e = 0;


    TLogReader t(p1);

#define ASSERT_LOC                              \
    e = t.nextAny();                            \
    ASSERT_TRUE(e);                             \
    ASSERT_TRUE(e->isLocation());               \
    ASSERT_FALSE(e->isTLogCRC());               \
    ASSERT_FALSE(e->isSCOCRC());                \
    ASSERT_FALSE(e->isSync());                  \
    ASSERT_EQ(0U, e->clusterAddress());        \
    ASSERT_EQ(e->clusterLocation(),l);

#define ASSERT_SYNC                             \
    e = t.nextAny();                            \
    ASSERT_TRUE(e);                             \
    ASSERT_FALSE(e->isLocation());              \
    ASSERT_FALSE(e->isTLogCRC());               \
    ASSERT_FALSE(e->isSCOCRC());                \
    ASSERT_TRUE(e->isSync());

    ASSERT_LOC;
    ASSERT_SYNC;
    // ASSERT_LOC;
    // ASSERT_LOC;
    // ASSERT_SYNC;
    // ASSERT_SYNC;
    // ASSERT_LOC;

#undef ASSERT_LOC
#undef ASSERT_SYNC

}

TEST_F(TLogTest, checksums)
{
    fs::path p1 = yt::FileUtils::temp_path("tmp1");
    fs::remove(p1);
    ALWAYS_CLEANUP_FILE(p1);
    CheckSum checksum(drand48() * std::numeric_limits<CheckSum::value_type>::max());

    ClusterLocationAndHash l(ClusterLocation(10), VolManagerTestSetup::growWeed());

    {
        TLogWriter w(p1);
        w.add(0,l);
        w.add(checksum);
        w.add(0,l);
        w.add(0,l);
        w.add(checksum);
        w.add(checksum);
        w.add(0,l);
    }


    const Entry* e = 0;

    TLogReader t(p1);

#define ASSERT_LOC                              \
    e = t.nextAny();                            \
    ASSERT_TRUE(e);                             \
    ASSERT_TRUE(e->isLocation());               \
    ASSERT_FALSE(e->isTLogCRC());               \
    ASSERT_FALSE(e->isSCOCRC());                \
    ASSERT_FALSE(e->isSync());                  \
    ASSERT_EQ(0U, e->clusterAddress());         \
    ASSERT_EQ(e->clusterLocation(), l);

#define ASSERT_CRC                                      \
    e = t.nextAny();                                    \
    ASSERT_TRUE(e);                                     \
    ASSERT_FALSE(e->isLocation());                      \
    ASSERT_FALSE(e->isTLogCRC());                       \
    ASSERT_TRUE(e->isSCOCRC());                         \
    ASSERT_FALSE(e->isSync());                          \
    ASSERT_EQ(e->getCheckSum(), checksum.getValue())

    ASSERT_LOC;
    ASSERT_CRC;
    ASSERT_LOC;
    ASSERT_LOC;
    ASSERT_CRC;
    ASSERT_CRC;
    ASSERT_LOC;

#undef ASSERT_LOC
#undef ASSERT_CRC
}

TEST_F(TLogTest, syncchecksums)
{
    fs::path p1 = yt::FileUtils::temp_path("tmp1");
    fs::remove(p1);
    ALWAYS_CLEANUP_FILE(p1);
    std::list<CheckSum> css;

    const auto weed(VolManagerTestSetup::growWeed());
    ClusterLocationAndHash l(ClusterLocation(10), weed);
    {
        TLogWriter w(p1);

        w.add(0, l);
        w.sync();
        ASSERT_EQ(FileUtils::calculate_checksum(p1), w.getCheckSum());

        w.add(0, l);
        w.sync();
        ASSERT_EQ(FileUtils::calculate_checksum(p1), w.getCheckSum());

        css.push_back(w.getCheckSum());

        const auto tlog_crc(w.close());
        css.push_back(tlog_crc);
        ASSERT_EQ(tlog_crc, w.getCheckSum());
    }

    ASSERT_EQ(css.back(), FileUtils::calculate_checksum(p1));

    const Entry* e = 0;
    TLogReader t(p1);

#ifdef ENABLE_MD5_HASH
#define ASSERT_LOC_MD5 ASSERT_EQ(l.weed(), weed)
#else
#define ASSERT_LOC_MD5
#endif

#define ASSERT_LOC_NO_MD5                               \
    e = t.nextAny();                                    \
    ASSERT_TRUE(e);                                     \
    ASSERT_TRUE(e->isLocation());                       \
    ASSERT_FALSE(e->isTLogCRC());                       \
    ASSERT_FALSE(e->isSCOCRC());                        \
    ASSERT_FALSE(e->isSync());                          \
    ASSERT_EQ(0U, e->clusterAddress());                  \
    ASSERT_EQ(l.clusterLocation, e->clusterLocation());

#define ASSERT_LOC ASSERT_LOC_NO_MD5 ASSERT_LOC_MD5

#define ASSERT_CLOSE(value)                              \
    e = t.nextAny();                                     \
    ASSERT_TRUE(e);                                      \
    ASSERT_FALSE(e->isLocation());                       \
    ASSERT_TRUE(e->isTLogCRC());                         \
    ASSERT_FALSE(e->isSCOCRC());                         \
    ASSERT_FALSE(e->isSync());                           \
    ASSERT_EQ(e->getCheckSum(), (value).getValue())

    ASSERT_LOC;
    ASSERT_LOC;
    ASSERT_CLOSE(css.front());

#undef ASSERT_LOC
#undef ASSERT_SYNC
}

// AR: could be merged with the previous test
TEST_F(TLogTest, forth_and_back)
{
    const fs::path p(yt::FileUtils::temp_path("temp_tlog"));
    fs::remove(p);

    youtils::Weed weed(VolManagerTestSetup::growWeed());
    const ClusterLocationAndHash clh(ClusterLocation(1), weed);
    const youtils::CheckSum cs(0);

    ALWAYS_CLEANUP_FILE(p);
    {
        TLogWriter w(p);
        w.add(0, clh);
        w.add(cs);
        w.sync();
        w.close();
    }

    TLogReader r(p);

    const Entry* e = r.nextAny();
    ASSERT(e != nullptr);

    EXPECT_TRUE(e->isLocation());
    EXPECT_FALSE(e->isSCOCRC());
    EXPECT_FALSE(e->isSync());
    EXPECT_FALSE(e->isTLogCRC());

    EXPECT_EQ(0U, e->clusterAddress());
    EXPECT_EQ(clh.clusterLocation, e->clusterLocation());
    EXPECT_EQ(clh.weed(), e->clusterLocationAndHash().weed());

    e = r.nextAny();
    ASSERT(e != nullptr);

    EXPECT_FALSE(e->isLocation());
    EXPECT_TRUE(e->isSCOCRC());
    EXPECT_FALSE(e->isSync());
    EXPECT_FALSE(e->isTLogCRC());

    EXPECT_EQ(cs, CheckSum(e->getCheckSum()));

    e = r.nextAny();
    ASSERT(e != nullptr);

    EXPECT_FALSE(e->isLocation());
    EXPECT_FALSE(e->isSCOCRC());
    EXPECT_FALSE(e->isSync());
    EXPECT_TRUE(e->isTLogCRC());

    ASSERT_TRUE(r.nextAny() == nullptr);
}

}

// Local Variables: **
// mode: c++ **
// End: **
