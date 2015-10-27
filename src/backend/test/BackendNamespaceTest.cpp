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

#include "../BackendTestSetup.h"
#include "../BackendException.h"
#include "../BackendConnectionInterface.h"
#include "BackendTestBase.h"

namespace backend
{

class BackendNamespaceTest
    : public BackendTestBase
{
public:
    BackendNamespaceTest()
        : BackendTestBase("BackendNamespaceTest")
        , ns_(youtils::UUID().str())
    {}

    bool
    namespaceExists(const Namespace& nspace)
    {
        BackendConnectionInterfacePtr conn(cm_->getConnection());
        // conn->timeout(BackendTestSetup::backend_timeout());
        bool ret = conn->namespaceExists(nspace);
        return ret;
    }

    void
    deleteNamespace(const Namespace& nspace)
    {
        BackendConnectionInterfacePtr conn(cm_->getConnection());
        // conn->timeout(BackendTestSetup::backend_timeout());
        conn->deleteNamespace(nspace);
    }

    void
    createNamespace(const Namespace& nspace)
    {
        BackendConnectionInterfacePtr conn = cm_->getConnection();
        // conn->timeout(BackendTestSetup::backend_timeout());
        conn->createNamespace(nspace);
    }

    void
    listNamespaces(std::list<std::string>& nspaces)
    {
        BackendConnectionInterfacePtr conn = cm_->getConnection();
        // conn->timeout(BackendTestSetup::backend_timeout());
        conn->listNamespaces(nspaces);
    }

    void
    namespaceRun(uint64_t testAmount)
    {
        std::list<Namespace> testNamespaces(testAmount);
        std::list<std::string> namespaces;

        listNamespaces(namespaces);
        uint32_t originalLength = namespaces.size();

        for (auto& it: testNamespaces)
        {
            createNamespace(it);
        }
        namespaces.clear();
        listNamespaces(namespaces);
        ASSERT_EQ(originalLength + testAmount, namespaces.size());

        for (auto& it: testNamespaces)
        {
            deleteNamespace(it);
        }
        namespaces.clear();
        listNamespaces(namespaces);
        ASSERT_EQ(originalLength, namespaces.size());
    }

    protected:
    const Namespace ns_;

private:
    DECLARE_LOGGER("BackendNamespaceTest");
};

TEST_F(BackendNamespaceTest, existence)
{
    ASSERT_FALSE(namespaceExists(Namespace(ns_)));
}

TEST_F(BackendNamespaceTest, ns)
{
    ASSERT_FALSE(namespaceExists(ns_));
    ASSERT_THROW(deleteNamespace(ns_),
                 BackendCouldNotDeleteNamespaceException);

    // use createTestNamespace here to make sure it's cleaned up after the test
    {

        std::unique_ptr<WithRandomNamespace> nspace = make_random_namespace(ns_.str());
        ASSERT_EQ(nspace->ns(), ns_);

        ASSERT_TRUE(namespaceExists(nspace->ns()));

        ASSERT_THROW(createNamespace(nspace->ns()),
                     BackendCouldNotCreateNamespaceException);
        //    deleteTestNamespace(nspace);
    }

    ASSERT_FALSE(namespaceExists(ns_));
    // create it once more
    {
        std::unique_ptr<WithRandomNamespace> nspace = make_random_namespace(ns_.str());
        //        createTestNamespace(nspace);

        ASSERT_TRUE(namespaceExists(nspace->ns()));
        // deleteTestNamespace(nspace);
    }

}

TEST_F(BackendNamespaceTest, list)
{
    namespaceRun(50);
    namespaceRun(200);
}

}

// Local Variables: **
// mode: c++ **
// End: **
