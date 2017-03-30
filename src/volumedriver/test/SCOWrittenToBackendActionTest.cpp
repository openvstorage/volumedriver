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


#include "../SCOWrittenToBackendAction.h"

#include "VolManagerTestSetup.h"

#include <sys/mman.h>

#include <youtils/ScopeExit.h>

namespace volumedrivertest
{

using namespace volumedriver;

namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace yt = youtils;

class SCOWrittenToBackendActionTest
    : public VolManagerTestSetup
{
protected:
    SCOWrittenToBackendActionTest()
        : VolManagerTestSetup("SCOWrittenToBackendActionTest")
    {}

    void
    set_action(SCOWrittenToBackendAction a)
    {
        bpt::ptree pt;
        VolManager& vm = *VolManager::get();
        vm.persistConfiguration(pt);

        ip::PARAMETER_TYPE(sco_written_to_backend_action)(a).persist(pt);
        UpdateReport urep;
        ConfigurationReport crep;

        vm.updateConfiguration(pt,
                               urep,
                               crep);

        EXPECT_EQ(a,
                  vm.get_sco_written_to_backend_action());
    }

    void
    verify_absence_from_page_cache(Volume& v, SCO sco)
    {
        CachedSCOPtr csp(VolManager::get()->getSCOCache()->findSCO(v.getNamespace(),
                                                                   sco));
        ASSERT_TRUE(csp != nullptr);

        const size_t page_size = sysconf(_SC_PAGESIZE);
        ASSERT_LT(0,
                  page_size);

        const size_t size = csp->getSize();
        ASSERT_EQ(0,
                  size % page_size);

        const int fd = ::open(csp->path().string().c_str(),
                              O_RDONLY);

        ASSERT_LE(0,
                  fd);

        auto close_fd(yt::make_scope_exit([&]
                                          {
                                              ::close(fd);
                                          }));

        void* m = ::mmap(0,
                         size,
                         PROT_READ,
                         MAP_PRIVATE,
                         fd,
                         0);
        ASSERT_TRUE(m != MAP_FAILED);

        auto unmap_fd(yt::make_scope_exit([&]
                                          {
                                              ::munmap(m,
                                                       size);
                                          }));

        std::vector<uint8_t> vec((size + page_size - 1) / page_size);

        ASSERT_EQ(0,
                  ::mincore(m,
                            size,
                            vec.data()));

        for (auto& b : vec)
        {
            EXPECT_EQ(0,
                      b);
        }
    }

    using SCOSet = std::set<SCO>;

    void
    check_disposables_in_set(SCONameList scol,
                             SCOSet scos)
    {
        for (const auto& s : scol)
        {
            EXPECT_EQ(1,
                      scos.erase(s));
        }

        EXPECT_TRUE(scos.empty());
    }

    using CheckFun = std::function<void(Volume&,
                                        SCOSet)>;

    void
    test(SCOWrittenToBackendAction a,
         CheckFun check_fun)
    {
        set_action(a);

        auto wrns(make_random_namespace());
        SharedVolumePtr v(newVolume(*wrns));

        const TLogMultiplier tm(v->getEffectiveTLogMultiplier());

        ASSERT_NE(TLogMultiplier(0),
                  tm);

        const SCOMultiplier sm(v->getSCOMultiplier());
        const ClusterSize cs(v->getClusterSize());

        SCONameList scos;
        SCOCache& sco_cache = *VolManager::get()->getSCOCache();

        {
            SCOPED_BLOCK_BACKEND(*v);

            writeToVolume(*v,
                          Lba(0),
                          cs * sm * tm,
                          "of no importance whatsoever");

            sco_cache.getSCONameList(wrns->ns(),
                                     scos,
                                     true);

            ASSERT_TRUE(scos.empty());

            sco_cache.getSCONameList(wrns->ns(),
                                     scos,
                                     false);

            ASSERT_FALSE(scos.empty());
        }

        waitForThisBackendWrite(*v);

        SCOSet scoset;
        for (const auto& s : scos)
        {
            auto res(scoset.insert(s));
            ASSERT_TRUE(res.second);
        }

        // don't forget to remove the current SCO from the set:
        ASSERT_FALSE(scoset.empty());
        scoset.erase(*scoset.rbegin());

        EXPECT_EQ(tm,
                  scoset.size());

        check_fun(*v,
                  std::move(scoset));
    }
};

TEST_P(SCOWrittenToBackendActionTest, enum_streaming)
{
    auto check([](const SCOWrittenToBackendAction a)
               {
                   std::stringstream ss;
                   ss << a;
                   EXPECT_TRUE(ss.good());

                   SCOWrittenToBackendAction b = SCOWrittenToBackendAction::SetDisposable;
                   ss >> b;

                   EXPECT_FALSE(ss.bad());
                   EXPECT_FALSE(ss.fail());
                   EXPECT_TRUE(ss.eof());

                   EXPECT_EQ(a, b);
               });

    check(SCOWrittenToBackendAction::SetDisposable);
    check(SCOWrittenToBackendAction::SetDisposableAndPurgeFromPageCache);
    check(SCOWrittenToBackendAction::PurgeFromSCOCache);
}

TEST_P(SCOWrittenToBackendActionTest, enum_streaming_failure)
{
    std::stringstream ss("fail");
    SCOWrittenToBackendAction a = SCOWrittenToBackendAction::SetDisposable;
    ss >> a;

    EXPECT_TRUE(ss.fail());
    EXPECT_EQ(SCOWrittenToBackendAction::SetDisposable,
              a);
}

TEST_P(SCOWrittenToBackendActionTest, set_disposable)
{
    test(SCOWrittenToBackendAction::SetDisposable,
         [&](Volume& v,
             SCOSet scos)
         {
             SCONameList disposables;
             VolManager::get()->getSCOCache()->getSCONameList(v.getNamespace(),
                                                              disposables,
                                                              true);
             check_disposables_in_set(std::move(disposables),
                                      std::move(scos));
         });
}

TEST_P(SCOWrittenToBackendActionTest, set_disposable_and_drop_from_page_cache)
{
    test(SCOWrittenToBackendAction::SetDisposableAndPurgeFromPageCache,
         [&](Volume& v,
             SCOSet scos)
         {
             SCONameList disposables;
             VolManager::get()->getSCOCache()->getSCONameList(v.getNamespace(),
                                                              disposables,
                                                              true);

             for (const auto& sco : disposables)
             {
                 verify_absence_from_page_cache(v,
                                                sco);
             }

             check_disposables_in_set(std::move(disposables),
                                      std::move(scos));

         });
}

TEST_P(SCOWrittenToBackendActionTest, purge_from_sco_cache)
{
    test(SCOWrittenToBackendAction::PurgeFromSCOCache,
         [&](Volume& v,
             SCOSet)
         {
             SCONameList disposables;
             VolManager::get()->getSCOCache()->getSCONameList(v.getNamespace(),
                                                              disposables,
                                                              true);
             EXPECT_TRUE(disposables.empty());
         });
}

INSTANTIATE_TEST_CASE_P(SCOWrittenToBackendActionTests,
                        SCOWrittenToBackendActionTest,
                        ::testing::Values(VolManagerTestSetup::default_test_config()));

}
