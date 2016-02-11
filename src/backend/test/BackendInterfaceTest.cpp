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

#include "BackendTestBase.h"

#include <boost/property_tree/ptree.hpp>

#include <youtils/Timer.h>

namespace backendtest
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace yt = youtils;

class BackendInterfaceTest
    : public be::BackendTestBase
{
public:
    BackendInterfaceTest()
        : be::BackendTestBase("BackendInterfaceTest")
    {}
};

// OVS-2204: there were errors even on retry with a new connection which actually
// should have succeeded - try to get to the bottom of that
TEST_F(BackendInterfaceTest, retry_on_error)
{
    const std::string oname("some-object");
    const fs::path opath(path_ / oname);

    const yt::CheckSum cs(createTestFile(opath,
                                         4096,
                                         "some pattern"));

    yt::CheckSum wrong_cs(1);

    ASSERT_NE(wrong_cs,
              cs);

    std::unique_ptr<be::BackendTestSetup::WithRandomNamespace>
        nspace(make_random_namespace());

    const uint32_t retries = 5;

    ASSERT_LE(retries,
              cm_->capacity());

    be::BackendInterfacePtr bi(cm_->newBackendInterface(nspace->ns()));

    const uint32_t secs = 1;

    {
        bpt::ptree pt;
        cm_->persist(pt);

        ip::PARAMETER_TYPE(backend_interface_retries_on_error)(retries).persist(pt);
        ip::PARAMETER_TYPE(backend_interface_retry_interval_secs)(secs).persist(pt);

        yt::ConfigurationReport crep;
        ASSERT_TRUE(cm_->checkConfig(pt,
                                     crep));

        yt::UpdateReport urep;
        cm_->update(pt,
                    urep);
    }

    yt::SteadyTimer t;

    ASSERT_THROW(bi->write(opath,
                           oname,
                           OverwriteObject::F,
                           &wrong_cs),
                 be::BackendInputException);

    EXPECT_EQ(retries + 1,
              cm_->size());

    EXPECT_LE(boost::chrono::seconds(retries * secs),
              t.elapsed());
}

}
