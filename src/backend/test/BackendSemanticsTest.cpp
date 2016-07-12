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

#include "BackendTestSetup.h"
#include "backend/BackendException.h"
#include "backend/BackendConnectionInterface.h"
#include "youtils/test/TestBase.h"

namespace backendtest
{
using namespace backend;

class BackendSemanticsTest
    : public BackendTestSetup, public testing::Test
{
     public:
     BackendSemanticsTest()
     {}

    virtual void
    SetUp()
    {
        setUpTestBackend();
    }

    virtual void
    TearDown()
    {
        tearDownTestBackend();

    }
};

struct WithNamespace()
{
    WithBackend(BackendConnPtr bcp,
                const std::string& namespace)
    {
        BackendPolicypolicy bcp.
    }

    ~WithNamespace()
    {

    }

};


TEST_F(BackendSemanticsTest, namespaces)
{}

}

// Local Variables: **
// bvirtual-targets: ("target/bin/youtils_test") **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
