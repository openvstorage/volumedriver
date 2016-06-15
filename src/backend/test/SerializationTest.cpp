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

#include <youtils/StrongTypedString.h>
#include <youtils/TestBase.h>

#include "../AlbaConfig.h"
#include "../BackendConfig.h"
#include "../LocalConfig.h"
#include "../MultiConfig.h"
#include "../Namespace.h"
#include "../S3Config.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

STRONG_TYPED_STRING(TestStringType1, backendtest);

namespace backendtest
{
using namespace backend;

class SerializationTest
    : public youtilstest::TestBase
{
protected:
    void
    check_config(const BackendConfig& config)
    {
        boost::property_tree::ptree pt;
        config.persist_internal(pt,
                                ReportDefault::T);
        std::stringstream ss;
        write_json(ss,
                   pt);

        {
            std::unique_ptr<BackendConfig> bc = BackendConfig::makeBackendConfig(ss.str());
            EXPECT_TRUE(config == *bc.get()) << "Test failed for backend type " << config.name();
        }
    }
};

TEST_F(SerializationTest, all)
{
    // SMART FALLTROUGH -> everything is tested
    // But a warning to add a new backend to this test
    switch(BackendType::LOCAL)
    {
    case BackendType::LOCAL:
        {
            LocalConfig config("a_path",
                               0,
                               0);
            check_config(config);
        }
    case BackendType::S3:
        {

            const S3Config config(S3Flavour::S3,
                                  "a_host",
                                  80,
                                  "a_username",
                                  "a_password",
                                  true,
                                  UseSSL::F,
                                  SSLVerifyHost::T,
                                  "");
            check_config(config);
        }
    case BackendType::MULTI:
        {
            std::vector<std::unique_ptr<BackendConfig> > configs;
            for(uint64_t i = 0; i < 10; ++i)
            {
                configs.emplace_back(new LocalConfig("a_path",
                                                     0,
                                                     0));
            }

            MultiConfig config(configs);
            check_config(config);
        }
    case BackendType::ALBA:
        {
            const AlbaConfig config("alba_host",
                                    10,
                                    5);
            check_config(config);
        }
    }
}

}
