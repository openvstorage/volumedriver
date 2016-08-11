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

#include "../Uri.h"

#include <boost/lexical_cast.hpp>
#include <boost/optional/optional_io.hpp>

#include <gtest/gtest.h>

namespace youtilstest
{

using namespace std::literals::string_literals;
using namespace youtils;

class UriTest
    : public testing::Test
{};

TEST_F(UriTest, construction)
{
    const std::string scheme("scheme");
    const std::string host("host");

    const Uri u = Uri().scheme(scheme).host(host);

    EXPECT_EQ(u,
              boost::lexical_cast<Uri>(scheme + "://"s + host));
}

TEST_F(UriTest, invalid)
{
    auto check([&](const std::string& s)
               {
                   EXPECT_THROW(boost::lexical_cast<Uri>(s),
                                std::exception) << s;
               });

    check("");
    check("not an uri");
}

TEST_F(UriTest, complete)
{
    const std::string s("scheme://user:pass@host:1234/parent/child?key1=%2Ffoo%2Fbar&key2=foo%21bar&just-a-key#fragment");
    const Uri u(s);

    EXPECT_EQ("scheme",
              *u.scheme());
    EXPECT_EQ("user:pass",
              *u.user_info());
    EXPECT_EQ("host",
              *u.host());
    EXPECT_EQ(1234,
              *u.port());

    const std::vector<std::string> path = { "parent", "child" };
    EXPECT_EQ(path,
              u.path_segments());

    const Uri::Query query = {
        { "key1", "/foo/bar"s },
        { "key2", "foo!bar"s },
        { "just-a-key", boost::none }
    };

    EXPECT_EQ(query,
              u.query());

    EXPECT_EQ("fragment",
              *u.fragment());

    EXPECT_FALSE(u.absolute_path());
}

TEST_F(UriTest, minimal)
{
    const std::string s("scheme://host");
    const Uri u(s);

    EXPECT_EQ("scheme",
              *u.scheme());
    EXPECT_EQ(boost::none,
              u.user_info());
    EXPECT_EQ("host",
              *u.host());
    EXPECT_EQ(boost::none,
              u.port());
    EXPECT_TRUE(u.path_segments().empty());
    EXPECT_TRUE(u.query().empty());
    EXPECT_EQ(boost::none,
              u.fragment());
    EXPECT_FALSE(u.absolute_path());
}

TEST_F(UriTest, roundtrip)
{
    const std::string scheme("scheme");
    const std::string host("host");
    const uint16_t port = 1234;
    const std::vector<std::string> path = { "parent", "child" };
    const Uri::Query query = {
        { "foo-key", "foo-val"s },
        { "bar-key", "bar-val"s },
    };

    const Uri u = Uri()
        .scheme(scheme)
        .host(host)
        .port(port)
        .path_segments(path)
        .query(query)
        ;

    EXPECT_EQ(scheme,
              u.scheme());
    EXPECT_EQ(host,
              u.host());
    EXPECT_NE(boost::none,
              u.port());
    EXPECT_EQ(port,
              *u.port());
    EXPECT_EQ(path,
              u.path_segments());
    EXPECT_EQ(query,
              u.query());
    EXPECT_EQ(boost::none,
              u.user_info());
    EXPECT_EQ(boost::none,
              u.fragment());
    EXPECT_FALSE(u.absolute_path());

    const auto str(boost::lexical_cast<std::string>(u));
    EXPECT_EQ(u,
              boost::lexical_cast<Uri>(str));
}

TEST_F(UriTest, empty_host)
{
    const std::string s("scheme://:1234/");
    const Uri u(s);

    EXPECT_EQ("scheme"s,
              u.scheme());
    EXPECT_EQ(boost::none,
              u.user_info());
    EXPECT_EQ(boost::none,
              u.host());
    EXPECT_EQ(1234,
              *u.port());
    EXPECT_TRUE(u.path_segments().empty());
    EXPECT_TRUE(u.query().empty());
    EXPECT_EQ(boost::none,
              u.fragment());
}

TEST_F(UriTest, file_path)
{
    const std::string s("/some/file/path");
    const Uri u(s);
    EXPECT_EQ(boost::none,
              u.scheme());
    EXPECT_EQ(boost::none,
              u.host());
    EXPECT_EQ(boost::none,
              u.port());
    std::vector<std::string> p = { "some"s, "file"s, "path"s };
    EXPECT_EQ(p,
              u.path_segments());
    EXPECT_TRUE(u.query().empty());
    EXPECT_EQ(boost::none,
              u.fragment());
    EXPECT_TRUE(u.absolute_path());

    EXPECT_EQ(s,
              boost::lexical_cast<std::string>(u));
}

TEST_F(UriTest, absolute_path)
{
    const std::string s("scheme:/user:pass@host:1234/abs/path");
    const Uri u(s);

    EXPECT_TRUE(u.absolute_path());

    EXPECT_EQ(s,
              boost::lexical_cast<std::string>(u));
}

TEST_F(UriTest, no_path)
{
    const std::string s("scheme://host:1234/");
    const Uri u(s);

    EXPECT_FALSE(u.absolute_path());
    EXPECT_TRUE(u.path_segments().empty());
}

}
