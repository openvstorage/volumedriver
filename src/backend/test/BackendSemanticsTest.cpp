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

#include "BackendTestSetup.h"
#include "backend/BackendException.h"
#include "backend/BackendConnectionInterface.h"
#include "youtils/test/TestBase.h"

namespace backendtest
{
using namespace backend;

class BackendSemanticsTest
    : public BackendTestSetup, public youtilstest::TestBase
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
