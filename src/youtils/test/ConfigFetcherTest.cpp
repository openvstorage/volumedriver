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

#include "../ConfigFetcher.h"
#include "../System.h"
#include "../Uri.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <gtest/gtest.h>

namespace youtilstest
{

using namespace std::literals::string_literals;
using namespace youtils;

namespace bpt = boost::property_tree;

struct ConfigFetcherTest
    : public testing::Test
{};

TEST_F(ConfigFetcherTest, DISABLED_fetch_and_dump)
{
    const std::string uristr(System::get_env_with_default("CONFIG_FETCHER_TEST_URI",
                                                          ""s));

    std::unique_ptr<ConfigFetcher> fetcher(ConfigFetcher::create(Uri(uristr)));
    bpt::json_parser::write_json(std::cout,
                                 (*fetcher)(VerifyConfig::F));
}

}
