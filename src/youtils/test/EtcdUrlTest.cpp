// Copyright 2016 iNuron NV
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

#include "../EtcdUrl.h"
#include "../TestBase.h"

#include <boost/lexical_cast.hpp>

namespace youtilstest
{

using namespace youtils;
using namespace std::literals::string_literals;

class EtcdUrlTest
    : public TestBase
{};

TEST_F(EtcdUrlTest, full)
{
    const std::string h("host");
    const uint16_t p = EtcdUrl::default_port + 1;
    const std::string k("/key");

    const auto u(boost::lexical_cast<EtcdUrl>("etcd://"s +
                                              h + ":"s +
                                              boost::lexical_cast<std::string>(p) +
                                              k));

    EXPECT_EQ(h
              , u.host);
    EXPECT_EQ(p,
              u.port);
    EXPECT_EQ(k,
              u.key);
}

TEST_F(EtcdUrlTest, host_only)
{
    const std::string h("host");

    const auto u(boost::lexical_cast<EtcdUrl>("etcd://"s + h));

    EXPECT_EQ(h,
              u.host);
    EXPECT_EQ(EtcdUrl::default_port,
              u.port);
    EXPECT_EQ("/"s,
              u.key);
}

TEST_F(EtcdUrlTest, invalid)
{
    auto test([](const std::string& str)
              {
                  EXPECT_THROW(boost::lexical_cast<EtcdUrl>(str),
                               std::exception);
              });

    test("");
    test("etcd");
    test("etcd:/foo");
    test("etc://localhost:1234");
    test("etcd://:1234/");
}

TEST_F(EtcdUrlTest, roundtrip)
{
    const EtcdUrl url("localhost",
                      EtcdUrl::default_port + 1,
                      "/some_key");

    EXPECT_EQ(url,
              boost::lexical_cast<EtcdUrl>(boost::lexical_cast<std::string>(url)));
}

TEST_F(EtcdUrlTest, inequality)
{
    const EtcdUrl u("localhost");
    const EtcdUrl v(u.host,
                    u.port + 1,
                    u.key);

    EXPECT_NE(u, v);
}

TEST_F(EtcdUrlTest, is_one_or_not)
{
    EXPECT_TRUE(EtcdUrl::is_one("etcd://host:234/key"s));
    EXPECT_FALSE(EtcdUrl::is_one("etc://host:234/key"s));
}

}
