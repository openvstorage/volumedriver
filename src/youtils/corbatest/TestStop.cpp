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
