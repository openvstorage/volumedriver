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
    : public ExGTest
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
