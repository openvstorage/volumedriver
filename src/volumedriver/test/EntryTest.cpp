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

#include "../Entry.h"
#include "ExGTest.h"

#include "VolManagerTestSetup.h"

namespace volumedriver
{
class EntryTest
    : public ExGTest
{};

#define PRINT(x) std::cout << #x << ": " << Entry :: x << std::endl;

TEST_F(EntryTest, DISABLED_print_sizes)
{
    // PRINT(TYPE_SIZE);
    // PRINT(TIMESTAMP_SIZE);
    // PRINT(DATA_SIZE);
    // PRINT(TOTAL_SIZE);
    // PRINT(TYPE_OFFSET);
    // PRINT(TIMESTAMP_OFFSET);
    // PRINT(DATA_OFFSET);
    // PRINT(CLUSTERLOCATION_OFFSET);
    //PRINT(NEW_CLUSTERLOCATION_OFFSET);
    //    std::cout << "sizeof Entry::entry: "<< sizeof(Entry::entry) << std::endl;
    //    std::cout << "sizeof Entry: "<< sizeof(Entry) << std::endl;
    //    ASSERT_TRUE(sizeof(Entry::entry) == Entry::TOTAL_SIZE);
    // ASSERT_TRUE(sizeof(Entry) == Entry::TOTAL_SIZE);
}
#undef PRINT

namespace
{

struct U
{
    U(const Entry& e)
        : entry(e)
    {};

    union
    {
        ClusterAddress ca;
        Entry entry;
    };
};

static_assert(sizeof(U) == sizeof(Entry), "somethings seriously broken");

}

TEST_F(EntryTest, size)
{
    //   EXPECT_EQ(Entry::getDataSize(), 16);
    EXPECT_EQ(sizeof(Entry),Entry::getDataSize());
}

TEST_F(EntryTest, cluster_address_limit)
{
    const youtils::Weed w(VolManagerTestSetup::growWeed());
    const ClusterLocationAndHash clh(ClusterLocation(1), w);

    EXPECT_NO_THROW(Entry(Entry::max_valid_cluster_address(), clh));
    EXPECT_THROW(Entry(Entry::max_valid_cluster_address() + 1, clh),
                 std::exception);

    U u(Entry(Entry::max_valid_cluster_address(), clh));
    u.ca++;

    EXPECT_THROW(u.entry.clusterAddress(),
                 std::exception);
}

}

// namespace volumedriver
// Local Variables: **
// mode: c++ **
// End: **
