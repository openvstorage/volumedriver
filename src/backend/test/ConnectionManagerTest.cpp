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

#include "BackendTestBase.h"

#include <future>

#include <boost/thread/thread.hpp>

#include <youtils/System.h>
#include <youtils/Timer.h>
#include <youtils/UpdateReport.h>

namespace backendtest
{

namespace bc = boost::chrono;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace yt = youtils;

using namespace backend;

class ConnectionManagerTest
    : public BackendTestBase
{
public:
    ConnectionManagerTest()
        : BackendTestBase("ConnectionManagerTest")
    {}

    void
    test_get_conns(const boost::optional<Namespace>& nspace)
    {
        const size_t iterations =
            yt::System::get_env_with_default("CONNECTION_MANAGER_TEST_ITERATIONS",
                                             1000000ULL);

        yt::SteadyTimer t;
        for (size_t i = 0; i < iterations; ++i)
        {
            BackendConnectionInterfacePtr c(cm_->getConnection());
            ASSERT_TRUE(c != nullptr);
        }

        const yt::SteadyTimer::Clock::duration elapsed = t.elapsed();
        const double seconds =
            bc::duration_cast<bc::nanoseconds>(elapsed).count() /
            (1000.0 * 1000.0 * 1000.0);
        std::cout << iterations << " lookups with nspace " << nspace <<
            " took " << elapsed << " -> " << (iterations / seconds) <<
            " OPS" << std::endl;
    }
};

TEST_F(ConnectionManagerTest, limited_pool)
{
    std::async(std::launch::async,
               [&]
               {
                   const size_t cap = cm_->capacity();
                   const size_t pools = cm_->pools().size();

                   ASSERT_LT(0U, cap);

                   ASSERT_EQ(0U,
                             cm_->size());

                   for (size_t i = 0; i < cap; ++i)
                   {
                       cm_->getConnection(ForceNewConnection::T);
                   }

                   ASSERT_EQ(cap * pools,
                             cm_->size());

                   cm_->getConnection(ForceNewConnection::T);

                   ASSERT_EQ(cap / pools,
                             cm_->size());
               }).wait();
}

// OVS-2204: for some reason the count of open fds goes up after an error, but
//  valgrind does not report a leak. Try to figure our what's going on here.
TEST_F(ConnectionManagerTest, limited_pool_on_errors)
{
    std::async(std::launch::async,
               [&]
               {
                   const std::string oname("some-object");
                   const fs::path opath(path_ / oname);

                   const yt::CheckSum cs(createTestFile(opath,
                                                        4096,
                                                        "some pattern"));

                   yt::CheckSum wrong_cs(1);

                   ASSERT_NE(wrong_cs,
                             cs);

                   std::unique_ptr<BackendTestSetup::WithRandomNamespace>
                       nspace(make_random_namespace());

                   BackendInterfacePtr bi(bi_(nspace->ns()));
                   const size_t count = 2 * cm_->capacity();

                   for (size_t i = 0; i < count; ++i)
                   {
                       ASSERT_THROW(bi->write(opath,
                                              oname,
                                              OverwriteObject::F,
                                              &wrong_cs),
                                    BackendInputException);
                   }

                   ASSERT_EQ(cm_->capacity() * cm_->pools().size(),
                             cm_->size());
               }).wait();
}

TEST_F(ConnectionManagerTest, no_pool)
{
    cm_->getConnection(ForceNewConnection::T);
    ASSERT_EQ(1U,
              cm_->size());

    using CapacityParam = ip::PARAMETER_TYPE(backend_connection_pool_capacity);

    {
        const CapacityParam new_cap(0);

        bpt::ptree pt;
        new_cap.persist(pt);

        yt::UpdateReport urep;

        cm_->update(pt,
                    urep);
    }

    ASSERT_EQ(0U,
              cm_->capacity());
    ASSERT_EQ(0U,
              cm_->size());

    {
        bpt::ptree pt;
        cm_->persist(pt);
        const CapacityParam new_cap(pt);
        ASSERT_EQ(0U,
                  new_cap.value());
    }

    ASSERT_NO_THROW(cm_->getConnection(ForceNewConnection::F));
    ASSERT_NO_THROW(cm_->getConnection(ForceNewConnection::T));

    ASSERT_EQ(0U,
              cm_->size());
}

TEST_F(ConnectionManagerTest, get_connection_without_nspace)
{
    test_get_conns(boost::none);
}

TEST_F(ConnectionManagerTest, get_connection_with_nspace)
{
    auto wrns(make_random_namespace());
    test_get_conns(wrns->ns());
}

}
