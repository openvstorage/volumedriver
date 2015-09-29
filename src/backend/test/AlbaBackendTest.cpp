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
