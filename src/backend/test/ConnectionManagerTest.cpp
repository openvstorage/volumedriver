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

class ConnectionManagerTest
    : public be::BackendTestBase
{
public:
    ConnectionManagerTest()
        : be::BackendTestBase("ConnectionManagerTest")
    {}
};

TEST_F(ConnectionManagerTest, limited_pool)
{
    const size_t cap = cm_->capacity();
    ASSERT_LT(0, cap);

    ASSERT_EQ(0,
              cm_->size());

    for (size_t i = 0; i < cap; ++i)
    {
        cm_->getConnection(ForceNewConnection::T);
    }

    ASSERT_EQ(cap,
              cm_->size());

    cm_->getConnection(ForceNewConnection::T);

    ASSERT_EQ(cap,
              cm_->size());
}

// OVS-2204: for some reason the count of open fds goes up after an error, but
//  valgrind does not report a leak. Try to figure our what's going on here.
TEST_F(ConnectionManagerTest, limited_pool_on_errors)
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

    be::BackendInterfacePtr bi(bi_(nspace->ns()));
    const size_t count = 2 * cm_->capacity();

    for (size_t i = 0; i < count; ++i)
    {
        ASSERT_THROW(bi->write(opath,
                               oname,
                               OverwriteObject::F,
                               &wrong_cs),
                     be::BackendInputException);
    }

    ASSERT_EQ(cm_->capacity(),
              cm_->size());
}

TEST_F(ConnectionManagerTest, no_pool)
{
    cm_->getConnection(ForceNewConnection::T);
    ASSERT_EQ(1,
              cm_->size());

    using CapacityParam = ip::PARAMETER_TYPE(backend_connection_pool_capacity);

    {
        const CapacityParam new_cap(0);

        bpt::ptree pt;
        new_cap.persist(pt);

        yt::UpdateReport urep;

        cm_->update(pt,
                    urep);
    }

    ASSERT_EQ(0,
              cm_->capacity());
    ASSERT_EQ(0,
              cm_->size());

    {
        bpt::ptree pt;
        cm_->persist(pt);
        const CapacityParam new_cap(pt);
        ASSERT_EQ(0,
                  new_cap.value());
    }

    ASSERT_NO_THROW(cm_->getConnection(ForceNewConnection::F));
    ASSERT_NO_THROW(cm_->getConnection(ForceNewConnection::T));

    ASSERT_EQ(0,
              cm_->size());

}

}
