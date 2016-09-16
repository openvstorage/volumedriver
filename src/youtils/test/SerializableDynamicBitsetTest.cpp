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

#include <gtest/gtest.h>
#include "../SerializableDynamicBitset.h"
#include <../wall_timer.h>
#include <../FileUtils.h>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "../SourceOfUncertainty.h"

namespace volumedrivertest
{

using namespace youtils;

namespace
{
DECLARE_LOGGER("SerializableDynamicBitset");
}

class SerializableDynamicBitsetTest : public testing::Test
{
public:
    // void
    // SetUp()
    // {}

    // void
    // TearDown()
    // {}

    uint64_t
    getTestSize()
    {
        static const uint64_t max_test_size = 1 << 25;
        static const uint64_t min_test_size = 1 << 10;
        return sof(min_test_size, max_test_size);

        //        return r.IntegerC<uint64_t>(min_test_size, max_test_size);

    }

    inline bool
    getbool()
    {
        const uint64_t val = 100;

        return sof((const uint64_t)0,val) > 50;
    }

    SourceOfUncertainty sof;

    typedef boost::archive::binary_iarchive iarchive_type;
    typedef boost::archive::binary_oarchive oarchive_type;
};


TEST_F(SerializableDynamicBitsetTest, DISABLED_test1)
{
    uint64_t test_size = getTestSize();
    LOG_INFO("Test size is " << test_size);

    SerializableDynamicBitset bs(test_size);



    for(uint64_t i = 0; i < test_size; ++i)
    {
        bs[i] = getbool();
    }

    boost::filesystem::path p = FileUtils::temp_path("dynamic_bitset_serialization");
    ALWAYS_CLEANUP_FILE(p);


    boost::filesystem::ofstream os(p);

    oarchive_type o_archive(os);
    wall_timer wt;
    o_archive << bs;
    os.flush();
    double out_time = wt.elapsed();

    LOG_INFO("Serialization out " << out_time << " seconds, speed: " << (test_size / out_time));

    ASSERT_TRUE(os.good());

    SerializableDynamicBitset bs2;


    wt.restart();
    boost::filesystem::ifstream is(p);
    iarchive_type i_archive(is);
    i_archive >> bs2;
    double in_time = wt.elapsed();
    LOG_INFO("Serialization in " << in_time << " seconds, speed: " << (test_size / in_time));
    for(uint64_t i = 0; i < test_size; ++i)
    {
        ASSERT_TRUE(bs[i] == bs2[i]);
    }
}

}

// Local Variables: **
// End: **
