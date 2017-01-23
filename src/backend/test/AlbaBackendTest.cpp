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

#include "BackendTestBase.h"

#include "../AlbaConfig.h"

#include <alba/llio.h>

namespace backend
{

namespace albaconn
{

namespace bpt = boost::property_tree;
namespace ip = initialized_params;

class AlbaBackendTest
    : public BackendTestBase
{
public:
    AlbaBackendTest()
        : BackendTestBase("AlbaBackendTest")
    {}
};

TEST_F(AlbaBackendTest, proxy_gone)
{
    // TODO: it'd be nicer to check that no-one's listening
    // on that port.
    const AlbaConfig cfg("127.0.0.1",
                         23456,
                         1000);

    bpt::ptree pt;
    cfg.persist_internal(pt,
                         ReportDefault::F);

    BackendConnectionManagerPtr cm(BackendConnectionManager::create(pt,
                                                                    RegisterComponent::F));

    BackendConnectionInterfacePtr conn(cm->getConnection());

    std::list<std::string> l;

    EXPECT_THROW(conn->listNamespaces(l),
                 BackendConnectFailureException);
}

TEST_F(AlbaBackendTest, config)
{
    bpt::ptree pt;

    const std::string host("localhost");
    ip::PARAMETER_TYPE(alba_connection_host)(host).persist(pt);
    const uint16_t port = 1234;
    ip::PARAMETER_TYPE(alba_connection_port)(port).persist(pt);
    const uint16_t timeout = 13;
    ip::PARAMETER_TYPE(alba_connection_timeout)(timeout).persist(pt);
    const std::string preset("preset");
    ip::PARAMETER_TYPE(alba_connection_preset)(preset).persist(pt);
    const bool use_rora = true;
    ip::PARAMETER_TYPE(alba_connection_use_rora)(use_rora).persist(pt);
    const size_t manifest_cache_capacity = 1024;
    ip::PARAMETER_TYPE(alba_connection_rora_manifest_cache_capacity)(manifest_cache_capacity).persist(pt);
    const bool nullio = false;
    ip::PARAMETER_TYPE(alba_connection_rora_use_nullio)(nullio).persist(pt);
    const size_t pool_capacity = 15;
    ip::PARAMETER_TYPE(alba_connection_asd_connection_pool_capacity)(pool_capacity).persist(pt);

    const AlbaConfig cfg(pt);

#define V(x)                                    \
    cfg.alba_connection_##x.value()

    EXPECT_EQ(host, V(host));
    EXPECT_EQ(port, V(port));
    EXPECT_EQ(timeout, V(timeout));
    EXPECT_EQ(preset, V(preset));
    EXPECT_EQ(use_rora, V(use_rora));
    EXPECT_EQ(manifest_cache_capacity, V(rora_manifest_cache_capacity));
    EXPECT_EQ(nullio, V(rora_use_nullio));
    EXPECT_EQ(pool_capacity, V(asd_connection_pool_capacity));

#undef V

    const std::unique_ptr<BackendConfig> clone(cfg.clone());
    EXPECT_EQ(cfg,
              dynamic_cast<const AlbaConfig&>(*clone));
}

}

}
