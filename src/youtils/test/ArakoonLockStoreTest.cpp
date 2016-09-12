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

#include <gtest/gtest.h>

#include "../ArakoonLockStore.h"
#include "../ArakoonTestSetup.h"
#include "../FileUtils.h"

namespace youtilstest
{
using namespace youtils;

namespace ara = arakoon;
namespace bptime = boost::posix_time;

class ArakoonLockStoreTest
    : public testing::Test
{
public:
    ArakoonLockStoreTest()
        : test_setup_(FileUtils::temp_path("ArakoonLockStoreTest") / "arakoon")
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

    const std::unique_ptr<UniqueObjectTag> ltag(als.write(lock,
                                                          nullptr));

    EXPECT_TRUE(als.exists());

    const std::tuple<HeartBeatLock, std::unique_ptr<UniqueObjectTag>> t(als.read());

    EXPECT_TRUE(lock.same_owner(std::get<0>(t)));

    EXPECT_EQ(*ltag,
              *std::get<1>(t));

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

    const std::unique_ptr<UniqueObjectTag> ltag1(als.write(lock1,
                                                           nullptr));

    const HeartBeatLock lock2(bptime::seconds(1),
                              bptime::seconds(1));

    EXPECT_THROW(als.write(lock2,
                           nullptr),
                 std::exception);

    const std::unique_ptr<UniqueObjectTag> ltag2(als.write(lock2,
                                                           ltag1.get()));

    EXPECT_NE(*ltag1,
              *ltag2);

    auto check([&](const HeartBeatLock& exp_lock,
                   const UniqueObjectTag& exp_ltag,
                   const HeartBeatLock& other_lock,
                   const UniqueObjectTag& other_ltag)
               {
                   const std::tuple<HeartBeatLock, std::unique_ptr<UniqueObjectTag>> t(als.read());
                   EXPECT_EQ(exp_ltag,
                             *std::get<1>(t));
                   EXPECT_NE(other_ltag,
                             *std::get<1>(t));

                   EXPECT_TRUE(exp_lock.same_owner(std::get<0>(t)));
                   EXPECT_FALSE(other_lock.same_owner(std::get<0>(t)));
               });

    check(lock2,
          *ltag2,
          lock1,
          *ltag1);

    EXPECT_THROW(als.write(lock1,
                           ltag1.get()),
                 GlobalLockStore::LockHasChanged);

    check(lock2,
          *ltag2,
          lock1,
          *ltag1);

    const std::unique_ptr<UniqueObjectTag> ltag3(als.write(lock1,
                                                           ltag2.get()));

    EXPECT_EQ(*ltag1,
              *ltag3) <<
        "ltag1 " << *ltag1 <<
        ", ltag2 " << *ltag2 <<
        ", ltag3 " << *ltag3;

    check(lock1,
          *ltag1,
          lock2,
          *ltag2);
}

}
