// Copyright 2015 Open vStorage NV
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

#include "VolManagerTestSetup.h"

namespace volumedriver
{

class MTVolumeTester
    : public VolManagerTestSetup
{
public:
    MTVolumeTester()
    // choose the scocache settings carefully, otherwise you might end up
    // with the tester not making any progress anymore. In particular, with
    // s = throttlespeed (CachedSCO, 4ms per cluster),
    // T = tlog time constant (SnapshotManagement, 60s),
    // N = tlog sco constant (SnapshotManagement, 20)
    // the following must be respected:
    // s * T << triggergap
    // N * scosize << triggergap
        : VolManagerTestSetup("MTVolumeTester",
                              UseFawltyMDStores::F,
                              UseFawltyTLogStores::F,
                              UseFawltyDataStores::F,
                              4, // num threads
                              "UNLIMITED", // scocache mp1 size
                              "UNLIMITED", // scocache mp2 size
                              "80MiB", // scocache trigger gap
                              "90MiB", // scocache backoff gap
                              1) // scocache cleanup interval (seconds)
    {}

    void
    mtRead(bool use_clustercache)
    {
        ASSERT_FALSE(use_clustercache);

        size_t size = 100 << 20; // MiB
        auto ns_ptr = make_random_namespace();

        const backend::Namespace& ns = ns_ptr->ns();

        Volume* v = newVolume("volume",
                              ns,
                              VolumeSize(size));



        //        v->setClusterCache(use_clustercache);


        const std::string pattern("12345678");
        writeToVolume(v, 0, size, pattern);

        boost::thread c1(boost::bind(&VolManagerTestSetup::checkVolume,
                                     v,
                                     pattern,
                                     v->getLBASize(),
                                     10));

        boost::thread c2(boost::bind(&VolManagerTestSetup::checkVolume,
                                     v,
                                     pattern,
                                     v->getClusterSize(),
                                     10));

        boost::thread c3(boost::bind(&VolManagerTestSetup::checkVolume,
                                     v,
                                     pattern,
                                     v->getClusterSize() * v->getSCOMultiplier(),
                                     10));

        c1.join();
        c2.join();
        c3.join();
    }
};

TEST_P(MTVolumeTester, test0)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = newVolume("volume1",
                           ns);

    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    Volume* v2 = newVolume("volume2",
                           ns2);

    VolumeWriteThread t1(v1,
                         "first",
                         100,
                         v1->getSize());

    VolumeWriteThread t2(v2,
                         "second",
                         100,
                         v2->getSize());

    t1.waitForFinish();
    t2.waitForFinish();
    // Write a VolumeChecker -> also parallel!
    checkVolume(v1,"first");
    checkVolume(v2,"second");
    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
    destroyVolume(v2,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}


// Y42 we should start documenting why these tests are disabled
TEST_P(MTVolumeTester, DISABLED_random)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = newVolume("volume1",
                           ns);

    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    Volume* v2 = newVolume("volume2",
			   ns2);

    VolumeWriteThread w1(v1,
                         "first",
                         100,
                         v1->getSize());

    VolumeWriteThread w2(v2,
                         "second",
                         100,
                         v2->getSize());

    w1.waitForFinish();
    w2.waitForFinish();

    VolumeRandomIOThread t1(v1,
                            "volume1");

    VolumeRandomIOThread t2(v2,
                            "volume2");

    sleep(300);

    t1.stop();
    t2.stop();
}

TEST_P(MTVolumeTester, mtReadNoClusterCache)
{
    mtRead(false);
}

TEST_P(MTVolumeTester, DISABLED_mtReadWithClusterCache)
{
    mtRead(true);
}

// not of much use except for valgrinding it
TEST_P(MTVolumeTester, mtReadWrite)
{
    size_t size = 50 << 20;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("volume",
                          ns,
                          VolumeSize(size));

    writeToVolume(v, "a");

    boost::thread w1(boost::bind(&VolManagerTestSetup::writeToVolume<Volume>,
                                 v,
                                 "w1",
                                 4096,
                                 5));

    boost::thread w2(boost::bind(&VolManagerTestSetup::writeToVolume<Volume>,
                                 v,
                                 "w2",
                                 4096,
                                 5));

    boost::thread r1(boost::bind(&VolManagerTestSetup::readVolume,
                                 this,
                                 v,
                                 v->getClusterSize(),
                                 10));

    boost::thread r2(boost::bind(&VolManagerTestSetup::readVolume,
                                 this,
                                 v,
                                 2 * v->getClusterSize(),
                                 10));

    w1.join();
    w2.join();
    r1.join();
    r2.join();

    syncToBackend(v);

    v->checkConsistency();
}

TEST_P(MTVolumeTester, snapshotting)
{
    size_t size = 100 << 20;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v = newVolume("volume",
                          ns,
                          VolumeSize(size));

    boost::thread w(boost::bind(&VolManagerTestSetup::writeToVolume<Volume>,
                                 v,
                                 "w1",
                                 4096,
                                 5));

    for (size_t i = 0; i < 100; ++i)
    {
        std::stringstream ss;
        ss << "snap-" << i;
        createSnapshot(v, ss.str());
        boost::this_thread::sleep(boost::posix_time::microseconds(1000));
        syncToBackend(v);
    }

    w.join();
    syncToBackend(v);

    v->checkConsistency();
}

INSTANTIATE_TEST(MTVolumeTester);
}

// Local Variables: **
// mode: c++ **
// End: **
