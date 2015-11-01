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
