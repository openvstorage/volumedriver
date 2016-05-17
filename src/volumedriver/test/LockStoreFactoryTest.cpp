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
