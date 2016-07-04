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
#include "ExGTest.h"

#include <iostream>
#include <memory>

#include <youtils/System.h>
#include <youtils/wall_timer.h>

#include "../PageSortingGenerator.h"

namespace volumedrivertest
{

namespace yt = youtils;
using namespace volumedriver;

class PageSortingGeneratorTest
    : public testing::TestWithParam<VolumeDriverTestConfig>
{};

TEST_F(PageSortingGeneratorTest, performance)
{
    const uint32_t num_tlogs = yt::System::get_env_with_default("NUM_TLOGS",
                                                                32U);
    const uint32_t entries_per_tlog = yt::System::get_env_with_default("TLOG_ENTRIES",
                                                                       1U << 20);
    const uint64_t vsize = yt::System::get_env_with_default("VOLSIZE",
                                                            1ULL << 30);
    const float randomness = yt::System::get_env_with_default("RANDOMNESS",
                                                              0.0);
    const uint64_t cached = yt::System::get_env_with_default("CACHED_CLUSTERS",
                                                             10ULL << 20);
    const uint64_t backend_delay = yt::System::get_env_with_default("BACKEND_DELAY_US",
                                                                    0ULL);

    LBAGenGen lgg(entries_per_tlog,
                  vsize,
                  randomness);

    std::unique_ptr<TLogGen>
        rg(new yt::RepeatGenerator<TLogGenItem, LBAGenGen>(lgg,
                                                           num_tlogs));

    std::unique_ptr<TLogGen>
        dg(new yt::DelayedGenerator<TLogGenItem>(std::move(rg),
                                                 backend_delay));

    auto ctr = std::make_shared<CombinedTLogReader>(std::move(dg));

    PageSortingGenerator_ psg(cached, ctr);

    yt::wall_timer wt;

    uint64_t pages = 0;
    uint64_t entries = 0;

    while (not psg.finished())
    {
        ++pages;
        PageData& d = *(psg.current());
        for (const auto& e : d)
        {
            (void)e;
            ++entries;
        }

        psg.next();
    }

    std::cout <<
        "TLog replay (volume size " << vsize <<
        ", randomness " << randomness <<
        ", backend delay " << backend_delay <<
        "us, " << num_tlogs << " TLogs of " << entries_per_tlog <<
        " entries each, pages " << pages <<
        ", entries " << entries <<
        ") took " << wt.elapsed() << " seconds" << std::endl;
}

}

// Local Variables: **
// mode: c++ **
// End: **
