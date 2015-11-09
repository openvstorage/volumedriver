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

#include "../ArakoonLockStore.h"
#include "../ArakoonTestSetup.h"
#include "../TestBase.h"

namespace youtilstest
{
using namespace youtils;

namespace ara = arakoon;
namespace bptime = boost::posix_time;

class ArakoonLockStoreTest
    : public TestBase
{
public:
    ArakoonLockStoreTest()
        : test_setup_(getTempPath("ArakoonLockStoreTest") / "arakoon")
    {
        test_setup_.setUpArakoon();
        larakoon_ = std::make_shared<LockedArakoon>(test_setup_.clusterID(),
                                                    test_setup_.node_configs());
    }

    ~ArakoonLockStoreTest()
    {
        larakoon_= nullptr;
        test_setup_.tearDownArakoon();
    }

protected:
    ara::ArakoonTestSetup test_setup_;
    std::shared_ptr<LockedArakoon> larakoon_;
};

TEST_F(ArakoonLockStoreTest, existentialism)
{
    const std::string nspace("some-namespace");

    ArakoonLockStore als(larakoon_,
                         nspace);

    EXPECT_FALSE(als.exists());
    EXPECT_THROW(als.read(),
                 std::exception);

    const HeartBeatLock lock(bptime::seconds(1),
                            bptime::seconds(1));

    const GlobalLockTag ltag(als.write(lock,
                                       boost::none));

    EXPECT_TRUE(als.exists());

    const std::tuple<HeartBeatLock, GlobalLockTag> t(als.read());

    EXPECT_TRUE(lock.same_owner(std::get<0>(t)));

    EXPECT_EQ(ltag,
              std::get<1>(t));

    als.erase();

    EXPECT_FALSE(als.exists());
}

TEST_F(ArakoonLockStoreTest, overwrite)
{
    const std::string nspace("some-namespace");

    ArakoonLockStore als(larakoon_,
                         nspace);

    const HeartBeatLock lock1(bptime::seconds(1),
                              bptime::seconds(1));

    const GlobalLockTag ltag1(als.write(lock1,
                                        boost::none));

    const HeartBeatLock lock2(bptime::seconds(1),
                              bptime::seconds(1));

    EXPECT_THROW(als.write(lock2,
                           boost::none),
                 std::exception);

    const GlobalLockTag ltag2(als.write(lock2,
                                        ltag1));

    EXPECT_NE(ltag1,
              ltag2);

    auto check([&](const HeartBeatLock& exp_lock,
                   const GlobalLockTag& exp_ltag,
                   const HeartBeatLock& other_lock,
                   const GlobalLockTag& other_ltag)
               {
                   const std::tuple<HeartBeatLock, GlobalLockTag> t(als.read());
                   EXPECT_EQ(exp_ltag,
                             std::get<1>(t));
                   EXPECT_NE(other_ltag,
                             std::get<1>(t));

                   EXPECT_TRUE(exp_lock.same_owner(std::get<0>(t)));
                   EXPECT_FALSE(other_lock.same_owner(std::get<0>(t)));
               });

    check(lock2,
          ltag2,
          lock1,
          ltag1);

    EXPECT_THROW(als.write(lock1,
                           ltag1),
                 GlobalLockStore::LockHasChanged);

    check(lock2,
          ltag2,
          lock1,
          ltag1);

    const GlobalLockTag ltag3(als.write(lock1,
                                        ltag2));

    EXPECT_EQ(ltag1,
              ltag3);

    check(lock1,
          ltag1,
          lock2,
          ltag2);
}

}
