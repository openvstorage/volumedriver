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

#include "ProtobufTest.pb.h"

#include <youtils/TestBase.h>

namespace volumedriverfstest
{

class ProtobufTest
    : public youtilstest::TestBase
{};

TEST_F(ProtobufTest, DISABLED_future_enum_values)
{
    // let's first see if the messages are really backward compatible.
    {
        OldMessage old_msg;
        old_msg.set_type(OldMessage_Type::OldMessage_Type_OldType);

        const std::string s(old_msg.SerializeAsString());

        NewMessage new_msg;
        new_msg.ParseFromString(s);

        EXPECT_EQ(NewMessage_Type::NewMessage_Type_OldType, new_msg.type());
    }

    // Now on to forward "compatibility":
    {
        NewMessage new_msg;
        new_msg.set_type(NewMessage_Type::NewMessage_Type_NewType);

        const std::string s(new_msg.SerializeAsString());

        OldMessage old_msg;
        old_msg.ParseFromString(s);
        EXPECT_THROW(old_msg.CheckInitialized(),
                     std::exception);
    }

    // The morale of the story: don't use enums in protobufs if you're not sure
    // that you don't need to enhance them in the future.
}

}
