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

#include <boost/property_tree/ptree.hpp>

#include <youtils/Timer.h>

namespace backendtest
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace yt = youtils;

class BackendInterfaceTest
    : public be::BackendTestBase
{
public:
    BackendInterfaceTest()
        : be::BackendTestBase("BackendInterfaceTest")
    {}
};

// OVS-2204: there were errors even on retry with a new connection which actually
// should have succeeded - try to get to the bottom of that
TEST_F(BackendInterfaceTest, retry_on_error)
{
    const std::string oname("some-object");
    const fs::path opath(path_ / oname);

    const yt::CheckSum cs(createTestFile(opath,
                                         4096,
                                         "some pattern"));

    yt::CheckSum wrong_cs(1);

    ASSERT_NE(wrong_cs,
              cs);

    std::unique_ptr<be::BackendTestSetup::WithRandomNamespace>
        nspace(make_random_namespace());

    const uint32_t retries = 5;

    ASSERT_LE(retries,
              cm_->capacity());

    be::BackendInterfacePtr bi(cm_->newBackendInterface(nspace->ns()));

    const uint32_t secs = 1;

    {
        bpt::ptree pt;
        cm_->persist(pt);

        ip::PARAMETER_TYPE(backend_interface_retries_on_error)(retries).persist(pt);
        ip::PARAMETER_TYPE(backend_interface_retry_interval_secs)(secs).persist(pt);

        yt::ConfigurationReport crep;
        ASSERT_TRUE(cm_->checkConfig(pt,
                                     crep));

        yt::UpdateReport urep;
        cm_->update(pt,
                    urep);
    }

    yt::SteadyTimer t;

    ASSERT_THROW(bi->write(opath,
                           oname,
                           OverwriteObject::F,
                           &wrong_cs),
                 be::BackendInputException);

    EXPECT_EQ(retries + 1,
              cm_->size());

    EXPECT_LE(boost::chrono::seconds(retries * secs),
              t.elapsed());
}

}
