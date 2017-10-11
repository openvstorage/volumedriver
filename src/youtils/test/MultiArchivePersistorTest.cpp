// Copyright (C) 2017 iNuron NV
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

#include "../MultiArchivePersistor.h"
#include "../Serialization.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <gtest/gtest.h>

namespace
{

struct CompatTest
{
    int value1 = 0;
    int value2 = 0;

    CompatTest(int i)
        : value1(i)
        , value2(2 * i)
    {}

    CompatTest() = default;

    bool
    operator==(const CompatTest& other) const
    {
        return
            value1 == other.value1 and
            value2 == other.value2;
    }

    template<typename Archive>
    void
    serialize(Archive& ar,
              const unsigned /* version */)
    {
        ar & boost::serialization::make_nvp("value1",
                                            value1);
        if (youtils::IsForwardCompatibleArchive<Archive>::value)
        {
            ar & boost::serialization::make_nvp("value2",
                                                value2);
        }
    }
};

}

namespace youtilstest
{

namespace ba = boost::archive;

using namespace youtils;

template<typename T>
struct MultiArchivePersistorTest
    : public testing::Test
{};

TYPED_TEST_CASE_P(MultiArchivePersistorTest);

TYPED_TEST_P(MultiArchivePersistorTest, compat_archive_roundtrip)
{
    using IArchives = LOKI_TYPELIST_2(TypeParam,
                                      ba::xml_iarchive);

    const CompatTest x(42);
    std::stringstream ss;
    const char* nvp = "CompatTest";

    MultiArchivePersistor<IArchives>::save(ss,
                                           nvp,
                                           x);

    CompatTest y;
    MultiArchivePersistor<IArchives>::load(ss,
                                           nvp,
                                           y);

    EXPECT_EQ(x,
              y);
}

TYPED_TEST_P(MultiArchivePersistorTest, compat_archive_old)
{
    using OldOArchive = typename AssociatedOArchive<TypeParam>::type;
    using IArchives = LOKI_TYPELIST_2(TypeParam,
                                      ba::xml_iarchive);

    const CompatTest x(43);
    std::stringstream ss;
    const char* nvp = "CompatTest";

    Serialization::serializeNVPAndFlush<OldOArchive>(ss,
                                                     nvp,
                                                     x);
    CompatTest y;
    MultiArchivePersistor<IArchives>::load(ss,
                                           nvp,
                                           y);

    EXPECT_EQ(x.value1,
              y.value1);
    EXPECT_NE(x.value2,
              y.value2);
    EXPECT_EQ(0,
              y.value2);
}

REGISTER_TYPED_TEST_CASE_P(MultiArchivePersistorTest,
                           compat_archive_roundtrip,
                           compat_archive_old);

using ArchiveTypes = testing::Types<ba::binary_iarchive,
                                    ba::text_iarchive>;

INSTANTIATE_TYPED_TEST_CASE_P(MultiArchivePersistorTests,
                              MultiArchivePersistorTest,
                              ArchiveTypes);

}
