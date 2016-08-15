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

#include "../ArakoonConfigFetcher.h"
#include "../ArakoonTestSetup.h"
#include "../ArakoonUrl.h"
#include "../InitializedParam.h"
#include "../FileUtils.h"
#include "../Uri.h"

#include <gtest/gtest.h>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace
{

const char* some_component = "some_component";

}

namespace initialized_params
{
DECLARE_INITIALIZED_PARAM(some_key,
                          std::string);

DEFINE_INITIALIZED_PARAM(some_key,
                         some_component,
                         "some_key",
                         "some key of some component",
                         ShowDocumentation::F);
}

namespace youtilstest
{

using namespace std::literals::string_literals;
using namespace youtils;

namespace ara = arakoon;
namespace bpt = boost::property_tree;
namespace ip = initialized_params;

class ArakoonConfigFetcherTest
    : public testing::Test
{
protected:
    ArakoonConfigFetcherTest()
        : test_setup_(FileUtils::temp_path("ArakoonConfigFetcherTest") / "arakoon")
    {}

    virtual void
    SetUp()
    {
        test_setup_.setUpArakoon();
    }

    virtual void
    TearDown()
    {
        test_setup_.tearDownArakoon();
    }

    void
    store_config(const std::string& path,
                 const bpt::ptree& pt)
    {
        std::stringstream ss;
        bpt::write_json(ss,
                        pt);

        std::unique_ptr<ara::Cluster> cluster(test_setup_.make_client());
        cluster->set(path,
                     ss.str());
    }

    ara::ArakoonTestSetup test_setup_;
};

TEST_F(ArakoonConfigFetcherTest, happy_path)
{
    const ip::PARAMETER_TYPE(some_key) key_stored("some_value");
    const std::string path("parent/child");
    const ArakoonUrl url(Uri("arakoon://"s +
                             test_setup_.clusterID() +
                             "/"s +
                             path +
                             "?ini=" +
                             test_setup_.server_config_file().generic_string()));
    {
        bpt::ptree pt;
        key_stored.persist(pt,
                           ReportDefault::T);

        store_config(path,
                     pt);
    }

    const bpt::ptree pt(ArakoonConfigFetcher(url)(VerifyConfig::F));

    ip::PARAMETER_TYPE(some_key) key_read(pt);

    EXPECT_EQ(key_stored.value(),
              key_read.value());
}

}
