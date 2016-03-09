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
        : VolManagerTestSetup("ThrottlingTest",
                              UseFawltyMDStores::F,
                              UseFawltyTLogStores::F,
                              UseFawltyDataStores::F,
                              4, // num threads
                              "1GiB", // scocache mp1 size
                              "1GiB", // sccache mp2 size
                              "100MiB", // scocache trigger gap
                              "150MiB", // scocache backoff gap
                              3600) // scocache cleanup interval (seconds)
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
               uint64_t lba,
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
                                0,
                                num_clusters * v->getClusterSize(),
                                "12345678");

        EXPECT_GT(expected_delay, t);

        throttle(true);

        t = timedWrite(v,
                       num_clusters * v->getClusterSize() / v->getLBASize(),
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
