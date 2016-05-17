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
    createNamespace(const Namespace& nspace,
                    const NamespaceMustNotExist must_not_exist = NamespaceMustNotExist::T)
    {
        BackendConnectionInterfacePtr conn = cm_->getConnection();
        // conn->timeout(BackendTestSetup::backend_timeout());
        conn->createNamespace(nspace,
                              must_not_exist);
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

TEST_F(BackendNamespaceTest, recreation)
{
    ASSERT_FALSE(namespaceExists(ns_));
    createNamespace(ns_);

    EXPECT_THROW(createNamespace(ns_,
                                 NamespaceMustNotExist::T),
                 BackendException);

    EXPECT_NO_THROW(createNamespace(ns_,
                                    NamespaceMustNotExist::F));
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
