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

}

}
