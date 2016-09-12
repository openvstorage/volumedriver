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

#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/property_tree/ptree.hpp>

#include <youtils/Timer.h>

namespace backendtest
{

using namespace backend;

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace yt = youtils;

class BackendInterfaceTest
    : public BackendTestBase
{
public:
    BackendInterfaceTest()
        : BackendTestBase("BackendInterfaceTest")
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

    std::unique_ptr<BackendTestSetup::WithRandomNamespace>
        nspace(make_random_namespace());

    const uint32_t retries = 5;

    ASSERT_LE(retries,
              cm_->capacity());

    BackendInterfacePtr bi(cm_->newBackendInterface(nspace->ns()));

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
                 BackendInputException);

    EXPECT_EQ(retries + 1,
              cm_->size());

    EXPECT_LE(boost::chrono::seconds(retries * secs),
              t.elapsed());
}

TEST_F(BackendInterfaceTest, unique_tag)
{
    std::unique_ptr<BackendTestSetup::WithRandomNamespace>
        nspace(make_random_namespace());
    BackendInterfacePtr bi(cm_->newBackendInterface(nspace->ns()));

    const std::string name("object");
    const fs::path p(path_ / "p");
    fs::ofstream(p) << p;

    std::unique_ptr<yt::UniqueObjectTag> tp(bi->write_tag(p,
                                                          name,
                                                          nullptr));

    EXPECT_NE(nullptr,
              tp);

    EXPECT_EQ(*tp,
              *(bi->get_tag(name)));

    const fs::path q(path_ / "q");
    fs::ofstream(q) << q;

    EXPECT_THROW(bi->write_tag(q,
                               name,
                               nullptr,
                               OverwriteObject::F),
                 BackendOverwriteNotAllowedException);

    EXPECT_EQ(*tp,
              *(bi->get_tag(name)));

    std::unique_ptr<yt::UniqueObjectTag> tq(bi->write_tag(q,
                                                          name,
                                                          tp.get(),
                                                          OverwriteObject::T));

    EXPECT_NE(*tp,
              *tq);
    EXPECT_EQ(*tq,
              *(bi->get_tag(name)));

    EXPECT_THROW(bi->write_tag(p,
                               name,
                               tp.get(),
                               OverwriteObject::T),
                 BackendUniqueObjectTagMismatchException);

    EXPECT_EQ(*tq,
              *(bi->get_tag(name)));

    const fs::path r(path_ / "r");
    fs::ofstream(r) << r;

    std::unique_ptr<yt::UniqueObjectTag> tr(bi->write_tag(r,
                                                          name,
                                                          nullptr,
                                                          OverwriteObject::T));

    EXPECT_NE(*tp,
              *tr);
    EXPECT_NE(*tq,
              *tr);
    EXPECT_EQ(*tr,
              *(bi->get_tag(name)));
}

TEST_F(BackendInterfaceTest, conditional_operations)
{
    std::unique_ptr<BackendTestSetup::WithRandomNamespace>
        nspace(make_random_namespace());
    BackendInterfacePtr bi(cm_->newBackendInterface(nspace->ns()));

    auto make_tag([&](const std::string& n) -> std::unique_ptr<yt::UniqueObjectTag>
                   {
                       const fs::path cpath(path_ / n);
                       fs::ofstream(cpath) << cpath;
                       return bi->write_tag(cpath,
                                            n,
                                            nullptr,
                                            OverwriteObject::T);
                   });

    const std::string cname("cond");
    const auto cond(boost::make_shared<Condition>(cname,
                                                  make_tag(cname)));
    const auto bogus_cond(boost::make_shared<Condition>(cname,
                                                        make_tag("another_cond")));

    const size_t fsize = 8192;
    const fs::path p(path_ / "p");
    const std::string patternp("PatternP");
    const yt::CheckSum chkp(createTestFile(p,
                                           fsize,
                                           patternp));

    const std::string oname("object");

    EXPECT_THROW(bi->write(p,
                           oname,
                           OverwriteObject::F,
                           &chkp,
                           bogus_cond),
                 BackendUniqueObjectTagMismatchException);

    EXPECT_FALSE(bi->objectExists(oname));

    bi->write(p,
              oname,
              OverwriteObject::F,
              &chkp,
              cond);

    EXPECT_TRUE(bi->objectExists(oname));

    EXPECT_THROW(bi->remove(oname,
                            ObjectMayNotExist::F,
                            bogus_cond),
                 BackendUniqueObjectTagMismatchException);

    EXPECT_TRUE(bi->objectExists(oname));

    const fs::path q(path_ / "q");
    const std::string patternq("PatternQ");
    const yt::CheckSum chkq(createTestFile(q,
                                           fsize + 1,
                                           patternq));

    EXPECT_THROW(bi->write(q,
                           oname,
                           OverwriteObject::T,
                           &chkq,
                           bogus_cond),
                 BackendUniqueObjectTagMismatchException);

    const fs::path r(path_ / "r");
    bi->read(r,
             oname,
             InsistOnLatestVersion::T);

    EXPECT_TRUE(verifyTestFile(r,
                               fsize,
                               patternp,
                               &chkp));

    bi->write(q,
              oname,
              OverwriteObject::T,
              &chkq,
              cond);

    bi->read(r,
             oname,
             InsistOnLatestVersion::T);

    EXPECT_TRUE(verifyTestFile(r,
                               fsize + 1,
                               patternq,
                               &chkq));

    bi->remove(oname,
               ObjectMayNotExist::F,
               cond);

    EXPECT_FALSE(bi->objectExists(oname));
}

}
