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

#include <boost/foreach.hpp>
#include <boost/scope_exit.hpp>
#include "../SCOCacheMountPoint.h"
#include "VolManagerTestSetup.h"
#include <youtils/wall_timer.h>
namespace volumedriver
{

class ThrottlingTest
    : public VolManagerTestSetup
{
public:
    ThrottlingTest()
        : VolManagerTestSetup(VolManagerTestSetupParameters("ThrottlingTest")
                              .sco_cache_trigger_gap("100MiB")
                              .sco_cache_backoff_gap("150MiB")
                              .sco_cache_cleanup_interval(3600))
    {}

    virtual void
    SetUp()
    {
        dstore_throttle_usecs_ = 50 * 1000;
        foc_throttle_usecs_ = 100 * 1000;
        VolManagerTestSetup::SetUp();
    }

    uint64_t
    timedWrite(SharedVolumePtr v,
               const Lba lba,
               size_t size,
               const std::string& pattern)
    {
        youtils::wall_timer i;
        //        i.start();
        writeToVolume(*v,
                      lba,
                      size,
                      pattern);
        //        i.stop();
        return i.elapsed() * 1000 * 1000;
    }

    template<typename T>
    void
    runTest(T throttle,
            SharedVolumePtr v,
            unsigned num_clusters,
            unsigned expected_delay)
    {
        uint64_t t = timedWrite(v,
                                Lba(0),
                                num_clusters * v->getClusterSize(),
                                "12345678");

        EXPECT_GT(expected_delay, t);

        throttle(true);

        t = timedWrite(v,
                       Lba(num_clusters * v->getClusterSize() / v->getLBASize()),
                       num_clusters * v->getClusterSize(),
                       "abcdefgh");

        EXPECT_LE(expected_delay, t);

        throttle(false);
    }

    void
    chokeMountPoints(bool choke)
    {
            BOOST_FOREACH(SCOCacheMountPointPtr m, getSCOCacheMountPointList()) {
                if(choke) {
                    m->setChoking(dstore_throttle_usecs_); }
                else {
                    m->setNoChoking(); }}
    }

    void
    chokeNamespace(const backend::Namespace& nspace,
                   bool choke)
    {
        typedef std::map<const backend::Namespace, SCOCacheNamespace*> NSMap;
        NSMap& m = getSCOCacheNamespaceMap();
        NSMap::iterator it = m.find(nspace);
        ASSERT_TRUE(it != m.end());

        it->second->setChoking(choke);
    }
};

TEST_P(ThrottlingTest, mountPointChoking)
{
    const std::string name("volume");
    auto ns_ptr = make_random_namespace();
    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v = newVolume(name,
                          ns);

    for (unsigned i = 1; i < 10; ++i)
    {
        runTest(boost::bind(&ThrottlingTest::chokeMountPoints,
                            this,
                            _1),
                v,
                i,
                i * dstore_throttle_usecs_);
    }
}

TEST_P(ThrottlingTest, namespaceChoking)
{
    const std::string name("volume");
    auto ns_ptr = make_random_namespace();
    const backend::Namespace& ns = ns_ptr->ns();
    SharedVolumePtr v = newVolume(name,
                          ns);

    for (unsigned i = 1; i < 10; ++i)
    {
        runTest(boost::bind(&ThrottlingTest::chokeNamespace,
                            this,
                            ns,
                            _1),
                v,
                i,
                i * dstore_throttle_usecs_);
    }
}

INSTANTIATE_TEST(ThrottlingTest);

}
// Local Variables: **
// mode: c++ **
// End: **
