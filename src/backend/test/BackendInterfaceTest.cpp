// Copyright 2015 Open vStorage NV
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

    const size_t retries = 7;
    ASSERT_LE(retries,
              cm_->capacity());

    be::BackendInterfacePtr bi(cm_->newBackendInterface(nspace->ns(),
                                                        retries));

    ASSERT_THROW(bi->write(opath,
                           oname,
                           OverwriteObject::F,
                           &wrong_cs),
                 be::BackendInputException);

    ASSERT_EQ(retries + 1,
              cm_->size());
}

}
