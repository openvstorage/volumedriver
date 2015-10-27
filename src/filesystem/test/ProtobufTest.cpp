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
