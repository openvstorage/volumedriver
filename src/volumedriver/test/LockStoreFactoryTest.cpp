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

#include "../LockStoreFactory.h"
#include "../VolumeDriverParameters.h"

#include <youtils/ArakoonLockStore.h>
#include <youtils/ArakoonTestSetup.h>
#include <youtils/TestBase.h>

#include <backend/BackendTestSetup.h>
#include <backend/LockStore.h>

namespace volumedrivertest
{

using namespace volumedriver;

namespace ara = arakoon;
namespace be = backend;
namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace yt = youtils;

class LockStoreFactoryTest
    : public youtilstest::TestBase
    , public be::BackendTestSetup
{
public:
    LockStoreFactoryTest()
        : ara_test_setup_(getTempPath("LockStoreFactoryTest") / "arakoon")

    {
        ara_test_setup_.setUpArakoon();
        initialize_connection_manager();
    }

    ~LockStoreFactoryTest()
    {
        uninitialize_connection_manager();
        ara_test_setup_.tearDownArakoon();
    }

    ara::ArakoonTestSetup ara_test_setup_;
};

TEST_F(LockStoreFactoryTest, defaults)
{
    bpt::ptree pt;
    LockStoreFactory lsf(pt,
                         RegisterComponent::F,
                         cm_);

    auto wrns(make_random_namespace());
    yt::GlobalLockStorePtr ls(lsf.build_one(wrns->ns()));
    EXPECT_TRUE(dynamic_cast<be::LockStore*>(ls.get()) != nullptr);
}

TEST_F(LockStoreFactoryTest, arakoon)
{
    bpt::ptree pt;

    ip::PARAMETER_TYPE(dls_type)(LockStoreType::Arakoon).persist(pt);

    EXPECT_THROW(LockStoreFactory(pt,
                                  RegisterComponent::F,
                                  cm_),
                 std::exception);

    ip::PARAMETER_TYPE(dls_arakoon_cluster_id)(ara_test_setup_.clusterID().str()).persist(pt);

    EXPECT_THROW(LockStoreFactory(pt,
                                  RegisterComponent::F,
                                  cm_),
                 std::exception);

    const auto node_configs(ara_test_setup_.node_configs());
    const ip::PARAMETER_TYPE(dls_arakoon_cluster_nodes)::ValueType
        node_vec(node_configs.begin(),
                 node_configs.end());

    ip::PARAMETER_TYPE(dls_arakoon_cluster_nodes)(node_vec).persist(pt);

    LockStoreFactory lsf(pt,
                         RegisterComponent::F,
                         cm_);

    auto wrns(make_random_namespace());

    yt::GlobalLockStorePtr ls(lsf.build_one(wrns->ns()));
    EXPECT_TRUE(dynamic_cast<yt::ArakoonLockStore*>(ls.get()) != nullptr);
}

}
