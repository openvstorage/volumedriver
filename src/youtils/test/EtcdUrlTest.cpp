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

#include "../EtcdUrl.h"

#include <gtest/gtest.h>

#include <boost/lexical_cast.hpp>
#include <boost/optional/optional_io.hpp>

namespace youtilstest
{

using namespace youtils;
using namespace std::literals::string_literals;

class EtcdUrlTest
    : public testing::Test
{};

TEST_F(EtcdUrlTest, full)
{
    const std::string h("host");
    const uint16_t p = *EtcdUrlTraits::default_port() + 1;
    const std::string k("key");

    const auto u(boost::lexical_cast<EtcdUrl>("etcd://"s +
                                              h + ":"s +
                                              boost::lexical_cast<std::string>(p) + "/"s +
                                              k));

    EXPECT_EQ(h,
              u.host());
    EXPECT_EQ(p,
              u.port());
    EXPECT_EQ(k,
              u.path());
}

TEST_F(EtcdUrlTest, host_only)
{
    const std::string h("host");

    const auto u(boost::lexical_cast<EtcdUrl>("etcd://"s + h));

    EXPECT_EQ(h,
              u.host());
    EXPECT_EQ(EtcdUrlTraits::default_port(),
              u.port());
    EXPECT_EQ(""s,
              u.path());
}

TEST_F(EtcdUrlTest, invalid)
{
    auto test([](const std::string& str)
              {
                  EXPECT_THROW(boost::lexical_cast<EtcdUrl>(str),
                               std::exception) << str;
              });

    test("");
    test("etcd");
    test("etc://localhost:1234");
}

TEST_F(EtcdUrlTest, roundtrip)
{
    const EtcdUrl url("etcd:://localhost:"s +
                      boost::lexical_cast<std::string>(*EtcdUrlTraits::default_port() + 1) +
                      "/"s + "some_key"s);

    EXPECT_EQ(url,
              boost::lexical_cast<EtcdUrl>(boost::lexical_cast<std::string>(url)));
}

TEST_F(EtcdUrlTest, inequality)
{
    const EtcdUrl u("etcd://localhost");
    const EtcdUrl v("etcd://"s + u.host() + ":"s +
                        boost::lexical_cast<std::string>(*u.port() + 1) +
                        "/"s + u.path());

    EXPECT_NE(u,
              v);
}

TEST_F(EtcdUrlTest, is_one_or_not)
{
    EXPECT_TRUE(EtcdUrl::is_one("etcd://host:234/key"s));
    EXPECT_FALSE(EtcdUrl::is_one("etc://host:234/key"s));
}

}
