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

#include "../TestBase.h"
#include "../OrbHelper.h"
#include "corba_youtils.h"
#include "../Assert.h"
#include "../CorbaTestSetup.h"
namespace youtils_test
{
using namespace youtils;

class CorbaYoutilsServant
{
public:
    CorbaYoutilsServant(OrbHelper& orb_helper)
        : orb_helper_(orb_helper)
    {}

    ~CorbaYoutilsServant()
    {}

    void
    stop()
    {
        orb_helper_.stop();
    }
private:
    OrbHelper& orb_helper_;
};


class TestStop : public youtilstest::TestBase
{
public:
    void
    SetUp()
    {
        orb_ = std::make_unique<OrbHelper>("something");
    }

    void
    TearDown()
    {

    }
    DECLARE_LOGGER("TestStop");


protected:
    std::unique_ptr<OrbHelper> orb_;

};

TEST_F(TestStop, test1)
{
    CorbaTestSetup test_setup("./corba_youtils_server");
    test_setup.start();

    CORBA::Object_var obj = orb_->getObjectReference("context_name",
                                                     "context_kind",
                                                     "object_name",
                                                     "object_kind");
    ASSERT_FALSE( CORBA::is_nil(obj));
    youtils_test::corba_test_var corba_test_ref_ = youtils_test::corba_test::_narrow(obj);
    ASSERT_FALSE(CORBA::is_nil(corba_test_ref_));
    corba_test_ref_->stop();
    sleep(1);

    EXPECT_FALSE(test_setup.running());


}


}
