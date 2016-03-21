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

#include "VolManagerTestSetup.h"

#include "../Api.h"
#include "../Scrubber.h"
#include "../ScrubberAdapter.h"
#include "../ScrubWork.h"
#include "../ScrubReply.h"
#include "../VolManager.h"

#include <boost/filesystem/fstream.hpp>

#include <backend/GarbageCollector.h>

namespace volumedrivertest
{

BOOLEAN_ENUM(CollectScrubGarbage);

using namespace scrubbing;
using namespace volumedriver;

class ScrubberTest
    : public VolManagerTestSetup
{
public:
    ScrubberTest()
        : VolManagerTestSetup("ScrubberTest")
    {}

    void
    setBackendType(ScrubberArgs& args)
    {
        args.backend_config = VolManager::get()->getBackendConfig().clone();
    }

    std::vector<scrubbing::ScrubWork>
    getScrubbingWork(const volumedriver::VolumeId& vid,
                     const boost::optional<SnapshotName>& start_snap = boost::none,
                     const boost::optional<SnapshotName>& end_snap = boost::none)
    {
        fungi::ScopedLock l(api::getManagementMutex());
        return api::getScrubbingWork(vid,
                                     start_snap,
                                     end_snap);
    }

    // This code is duplicate from the code in python_scrubber ->unify

    scrubbing::ScrubReply
    do_scrub(const scrubbing::ScrubWork& scrub_work,
             uint64_t region_size_exponent = 5,
             float fill_ratio = 1.0,
             bool apply_immediately = false,
             bool verbose_scrubbing = true)
    {
        return ScrubberAdapter::scrub(scrub_work,
                                      getTempPath(testName_),
                                      region_size_exponent,
                                      fill_ratio,
                                      apply_immediately,
                                      verbose_scrubbing);
    }

    void
    apply_scrubbing(const VolumeId& vid,
                    const scrubbing::ScrubReply& scrub_reply,
                    const ScrubbingCleanup cleanup = ScrubbingCleanup::Always,
                    const CollectScrubGarbage collect_garbage = CollectScrubGarbage::T)
    {
        fungi::ScopedLock l(api::getManagementMutex());

        SharedVolumePtr v(api::getVolumePointer(vid));
        const MaybeScrubId md_scrub_id(v->getMetaDataStore()->scrub_id());
        ASSERT_TRUE(md_scrub_id != boost::none);

        const ScrubId sp_scrub_id(v->getSnapshotManagement().scrub_id());

        ASSERT_EQ(sp_scrub_id,
                  *md_scrub_id);

        boost::optional<be::Garbage> garbage(v->applyScrubbingWork(scrub_reply,
                                                                   cleanup));

        if (garbage and collect_garbage == CollectScrubGarbage::T)
        {
            waitForThisBackendWrite(*v);

            be::GarbageCollectorPtr gc(api::backend_garbage_collector());
            gc->queue(std::move(*garbage));
            EXPECT_TRUE(gc->barrier(v->getNamespace()).get());
        }

        const MaybeScrubId md_scrub_id2(v->getMetaDataStore()->scrub_id());
        ASSERT_TRUE(md_scrub_id2 != boost::none);

        const ScrubId sp_scrub_id2(v->getSnapshotManagement().scrub_id());

        ASSERT_EQ(sp_scrub_id2,
                  *md_scrub_id2);

        ASSERT_NE(sp_scrub_id,
                  sp_scrub_id2);
    }
};

TEST_P(ScrubberTest, isScrubResultString)
{
    ASSERT_TRUE(Scrubber::isScrubbingResultString("scrubbing_result5fda6bdf-4df1-4dae-a488-b108447ced3d"));
}

TEST_P(ScrubberTest, DeletedSnap)
{
    VolumeId vid("volume1");

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    //    backend::Namespace ns;

    SharedVolumePtr v1 = newVolume(vid,
    			   ns);

    std::string what;

    for(int i = 0; i < 1024; ++i)
    {
        std::stringstream ss;
        ss << i;
        what = ss.str();

        writeToVolume(*v1, 0, default_cluster_size(),what);
        writeToVolume(*v1, 1 << 10, default_cluster_size(), what + "-" );
    }

    const SnapshotName snap1("snap1");

    v1->createSnapshot(snap1);
    waitForThisBackendWrite(*v1);
    auto scrub_work_units = getScrubbingWork(vid);

    ASSERT_EQ(1U, scrub_work_units.size());

    v1->deleteSnapshot(snap1);
    persistXVals(v1->getName());
    waitForThisBackendWrite(*v1);

    EXPECT_THROW(do_scrub(scrub_work_units.front()),
                 ::scrubbing::ScrubberException);
}

TEST_P(ScrubberTest, DeletedSnap2)
{
    // const backend::Namespace nspace;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    VolumeId vid("volume1");

    SharedVolumePtr v1 = newVolume(vid,
                           nspace);

    std::string what;

    for(int i = 0; i < 1024; ++i)
    {
        std::stringstream ss;
        ss << i;
        what = ss.str();

        writeToVolume(*v1, 0, default_cluster_size(), what);
        writeToVolume(*v1, 1 << 10, default_cluster_size(), what + "-" );
    }

    const SnapshotName snap1("snap1");

    v1->createSnapshot(snap1);
    waitForThisBackendWrite(*v1);
    auto scrub_work_units = getScrubbingWork(vid);

    ASSERT_EQ(1U, scrub_work_units.size());

    ScrubberArgs args;
    persistXVals(v1->getName());
    waitForThisBackendWrite(*v1);


    scrubbing::ScrubReply result;

    ASSERT_NO_THROW(result = do_scrub(scrub_work_units.front()));

    v1->deleteSnapshot(snap1);
    {
        fungi::ScopedLock l(api::getManagementMutex());
        EXPECT_THROW(apply_scrubbing(vid,
                                     result),
                     fungi::IOException);
    }
    waitForThisBackendWrite(*v1);
}

TEST_P(ScrubberTest, GetWork)
{
    //backend::Namespace ns;
    VolumeId vid("volume1");

    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(vid,
                           ns);

    std::string what;

    for(int i = 0; i < 1024; ++i)
    {
        std::stringstream ss;
        ss << i;
        what = ss.str();

        writeToVolume(*v1, 0, default_cluster_size(),what);
        writeToVolume(*v1, 1 << 10, default_cluster_size(), what + "-" );
    }

    {
        auto scrub_work_units = getScrubbingWork(vid);
        ASSERT_EQ(0U, scrub_work_units.size());
    }

    v1->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(*v1);
    {
        auto scrub_work_units = getScrubbingWork(vid);
        ASSERT_EQ(1U, scrub_work_units.size());
        const ScrubWork& w = scrub_work_units.front();
        ASSERT_TRUE(w.backend_config_.get());
        EXPECT_TRUE(*(w.backend_config_.get()) == VolManager::get()->getBackendConfig());
        EXPECT_EQ(w.ns_, ns);
        EXPECT_EQ(ilogb(default_cluster_size()), w.cluster_exponent_);
        EXPECT_EQ(32U, w.sco_size_);
    }
    auto scrub_work_units = getScrubbingWork(vid);
    {
        auto scrub_work_units = getScrubbingWork(vid);
        ASSERT_EQ(1U, scrub_work_units.size());
        const ScrubWork& w = scrub_work_units.front();
        EXPECT_EQ(w.ns_, ns);
        ASSERT_TRUE(w.backend_config_.get());
        EXPECT_TRUE(*(w.backend_config_.get()) == VolManager::get()->getBackendConfig());
        EXPECT_EQ(ilogb(default_cluster_size()), w.cluster_exponent_);
        EXPECT_EQ(32U, w.sco_size_);
    }

    ScrubberArgs args;
    persistXVals(v1->getName());
    waitForThisBackendWrite(*v1);

    scrubbing::ScrubReply scrub_result;

    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));

    ASSERT_NO_THROW(apply_scrubbing(vid,
                                    scrub_result));

    {
        auto scrub_work_units = getScrubbingWork(vid);
        ASSERT_EQ(0U, scrub_work_units.size());
    }

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(ScrubberTest, GetWork2)
{
    const VolumeId vid("volume");
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    auto v = newVolume(vid,
                       ns);

    const SnapshotName snapa("A");
    writeToVolume(*v, 0, default_cluster_size(), snapa);
    v->createSnapshot(snapa);
    waitForThisBackendWrite(*v);

    const SnapshotName snapb("B");
    writeToVolume(*v, 0, default_cluster_size(), snapb);
    v->createSnapshot(snapb);
    waitForThisBackendWrite(*v);

    const SnapshotName snapc("C");
    writeToVolume(*v, 0, default_cluster_size(), snapc);
    v->createSnapshot(snapc);
    waitForThisBackendWrite(*v);

    {
        auto scrub_work_units = getScrubbingWork(vid);
        ASSERT_EQ(3U, scrub_work_units.size());
        EXPECT_EQ(snapa, scrub_work_units[0].snapshot_name_);
        EXPECT_EQ(snapb, scrub_work_units[1].snapshot_name_);
        EXPECT_EQ(snapc, scrub_work_units[2].snapshot_name_);
    }

    {
        auto scrub_work_units = getScrubbingWork(vid,
                                                 boost::none,
                                                 snapa);
        ASSERT_EQ(1U, scrub_work_units.size());
        EXPECT_EQ(snapa, scrub_work_units[0].snapshot_name_);
    }

    {
        auto scrub_work_units = getScrubbingWork(vid,
                                                 boost::none,
                                                 snapb);

        ASSERT_EQ(2U, scrub_work_units.size());
        EXPECT_EQ(snapa, scrub_work_units[0].snapshot_name_);
        EXPECT_EQ(snapb, scrub_work_units[1].snapshot_name_);
    }

    {
        auto scrub_work_units = getScrubbingWork(vid,
                                                 boost::none,
                                                 snapc);

        ASSERT_EQ(3U, scrub_work_units.size());
        EXPECT_EQ(snapa, scrub_work_units[0].snapshot_name_);
        EXPECT_EQ(snapb, scrub_work_units[1].snapshot_name_);
        EXPECT_EQ(snapc, scrub_work_units[2].snapshot_name_);
    }

    {
        auto scrub_work_units = getScrubbingWork(vid,
                                                 snapa,
                                                 boost::none);
        ASSERT_EQ(2U, scrub_work_units.size());
        EXPECT_EQ(snapb, scrub_work_units[0].snapshot_name_);
        EXPECT_EQ(snapc, scrub_work_units[1].snapshot_name_);
    }

    {
        auto scrub_work_units = getScrubbingWork(vid,
                                                 snapa,
                                                 snapb);
        ASSERT_EQ(1U, scrub_work_units.size());
        EXPECT_EQ(snapb, scrub_work_units[0].snapshot_name_);
    }

    {
        auto scrub_work_units = getScrubbingWork(vid,
                                                 snapa,
                                                 snapa);
        ASSERT_EQ(0U, scrub_work_units.size());
    }

    {
        EXPECT_THROW(getScrubbingWork(vid,
                                      snapb,
                                      snapa),
                     fungi::IOException);
    }

    {
        EXPECT_THROW(getScrubbingWork(vid,
                                      SnapshotName("does_not_exists"),
                                      boost::none),
                     fungi::IOException);
    }

    {
        EXPECT_THROW(getScrubbingWork(vid,
                                      boost::none,
                                      SnapshotName("does_not_exists")),
                     fungi::IOException);
    }
}

TEST_P(ScrubberTest, Serialization)
{
    // const backend::Namespace ns;

    const VolumeId vid("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(vid,
                           ns);

    std::string what;

    for(int i = 0; i < 1024; ++i)
    {
        std::stringstream ss;
        ss << i;
        what = ss.str();

        writeToVolume(*v1, 0, default_cluster_size(),what);
        writeToVolume(*v1, 1 << 10, default_cluster_size(), what + "-" );
    }

    const SnapshotName snap1("snap1");
    v1->createSnapshot(snap1);
    waitForThisBackendWrite(*v1);
    EXPECT_EQ(2048U * default_cluster_size(),
              v1->getSnapshotBackendSize(snap1));

    persistXVals(v1->getName());
    waitForThisBackendWrite(*v1);
    auto scrub_work_units = getScrubbingWork(vid);
    ASSERT_EQ(1U, scrub_work_units.size());
    scrubbing::ScrubReply scrub_result;

    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));

    ASSERT_NO_THROW(apply_scrubbing(vid,
                                    scrub_result));

    checkVolume(*v1, 0, default_cluster_size(),what);
    checkVolume(*v1,1 << 10, default_cluster_size(), what + "-");
    EXPECT_EQ(default_cluster_size() * 2,
              v1->getSnapshotBackendSize(snap1));
}

TEST_P(ScrubberTest, ScrubNothing)
{
    // const backend::Namespace ns;
    const VolumeId vid("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(vid,
                           ns);
    v1->createSnapshot(SnapshotName("snap1"));
    waitForThisBackendWrite(*v1);
    auto scrub_work_units = getScrubbingWork(vid);
    ASSERT_EQ(1U, scrub_work_units.size());

    scrubbing::ScrubReply scrub_result;

    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));

    ASSERT_NO_THROW(apply_scrubbing(vid,
                                    scrub_result));

    {
        auto scrub_work_units = getScrubbingWork(vid);
        ASSERT_EQ(0U, scrub_work_units.size());
    }

    writeToVolume(*v1, 0, default_cluster_size(), "arner");
    checkVolume(*v1, 0, default_cluster_size(), "arner");
    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(ScrubberTest, SimpleScrub2)
{
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    const VolumeId vid("volume1");
    SharedVolumePtr v1 = newVolume("volume1",
                           ns);

    std::string what;

    for(int i = 0; i < 1024; ++i)
    {
        std::stringstream ss;
        ss << i;
        what = ss.str();

        writeToVolume(*v1, 0, default_cluster_size(),what);
    }

    v1->createSnapshot(SnapshotName("snap1"));
    persistXVals(v1->getName());
    waitForThisBackendWrite(*v1);

    auto scrub_work_units = getScrubbingWork(vid);
    ASSERT_EQ(1U, scrub_work_units.size());
    scrubbing::ScrubReply scrub_result;
    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));
    ASSERT_NO_THROW(apply_scrubbing(vid,
                                    scrub_result));
    checkVolume(*v1, 0, default_cluster_size(),what);
}

TEST_P(ScrubberTest, SimpleScrub3)
{
    // const backend::Namespace ns;
    const VolumeId vid("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume("volume1",
			   ns);

    std::string what;

    for(int i = 0; i < 1024; ++i)
    {
        std::stringstream ss;
        ss << i;
        what = ss.str();

        writeToVolume(*v1, 0, default_cluster_size(),what);
        writeToVolume(*v1, 1 << 10, default_cluster_size(), what + "-" );
    }

    v1->createSnapshot(SnapshotName("snap1"));

    persistXVals(v1->getName());
    waitForThisBackendWrite(*v1);

    auto scrub_work_units = getScrubbingWork(vid);
    ASSERT_EQ(1U, scrub_work_units.size());

    scrubbing::ScrubReply scrub_result;

    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));

    ASSERT_NO_THROW(apply_scrubbing(vid,
                                    scrub_result,
                                    ScrubbingCleanup::OnError));

    checkVolume(*v1, 0, default_cluster_size(),what);
    checkVolume(*v1,1 << 10, default_cluster_size(), what + "-");
    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);

}

TEST_P(ScrubberTest, SimpleScrub4)
{
    // const backend::Namespace ns;
    const VolumeId vid("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(vid,
                           ns);

    std::string what;

    for(int i = 0; i < 1024; ++i)
    {
        std::stringstream ss;
        ss << i;
        what = ss.str();

        writeToVolume(*v1, 0, default_cluster_size(),what);
        writeToVolume(*v1, 1 << 10, default_cluster_size(), what + "-" );
        writeToVolume(*v1, 1 << 15, default_cluster_size(), what + "--" );
    }

    v1->createSnapshot(SnapshotName("snap1"));

    persistXVals(v1->getName());
    waitForThisBackendWrite(*v1);

    const auto scrub_work_units = getScrubbingWork(vid);
    ASSERT_EQ(1U, scrub_work_units.size());

    scrubbing::ScrubReply scrub_result;
    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));

    ASSERT_NO_THROW(apply_scrubbing(vid,
                                    scrub_result,
                                    ScrubbingCleanup::OnError));
    checkVolume(*v1, 0, default_cluster_size(),what);
    checkVolume(*v1,1 << 10, default_cluster_size(), what + "-");
    checkVolume(*v1,1 << 15, default_cluster_size(), what + "--");
}

TEST_P(ScrubberTest, SmallRegionScrub1)
{
    // const backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    const VolumeId vid("volume1");
    SharedVolumePtr v1 = newVolume(vid,
                           ns);

    std::string what;

    for(int i = 0; i < 512; ++i)
    {
        std::stringstream ss;
        ss << i;
        what = ss.str();
        writeToVolume(*v1,
                      i * default_cluster_multiplier(),
                      default_cluster_size(),
                      what);
        writeToVolume(*v1,
                      513 * default_cluster_multiplier(),
                      default_cluster_size(),
                      "rest");
    }

    v1->createSnapshot(SnapshotName("snap1"));
    persistXVals(v1->getName());
    waitForThisBackendWrite(*v1);

    auto scrub_work_units = getScrubbingWork(vid);

    scrubbing::ScrubReply scrub_result;
    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));

    ASSERT_NO_THROW(apply_scrubbing(vid,
                                    scrub_result,
                                    ScrubbingCleanup::OnError));

    for(int i = 0; i < 512; ++i)
    {
        std::stringstream ss;
        ss << i;
        what = ss.str();

        checkVolume(*v1,
                    i * default_cluster_multiplier(),
                    default_cluster_size(),
                    what);
    }
    checkVolume(*v1,
                513 * default_cluster_multiplier(),
                default_cluster_size(),
                "rest");

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(ScrubberTest, CloneScrubbin)
{
    auto ns_ptr = make_random_namespace();
    const backend::Namespace& ns = ns_ptr->ns();

    const VolumeId vid("volume1");
    SharedVolumePtr v1 = newVolume("volume1",
                           ns);

    const int num_writes = 1024;

    for(int i =0; i < num_writes; ++i)
    {
        writeToVolume(*v1, 0, default_cluster_size(), "xxx");
        writeToVolume(*v1, default_cluster_multiplier(), default_cluster_size(), "xxx");
        writeToVolume(*v1, 2 * default_cluster_multiplier(), default_cluster_size(), "xxx");
    }

    writeToVolume(*v1, 0, default_cluster_size(), "bart");
    writeToVolume(*v1, default_cluster_multiplier(), default_cluster_size(), "arne");
    writeToVolume(*v1, 2 * default_cluster_multiplier(), default_cluster_size(), "immanuel");

    const SnapshotName v1_snap1("v1_snap1");

    v1->createSnapshot(v1_snap1);
    persistXVals(v1->getName());

    waitForThisBackendWrite(*v1);
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns1_ptr->ns();

    //const backend::Namespace ns1;
    const VolumeId clone1("clone1");

    SharedVolumePtr c1 = createClone(clone1,
                             ns1,
                             ns,
                             v1_snap1);

    for(int i = 0; i < num_writes; ++i)
    {
        writeToVolume(*c1,
                      default_cluster_multiplier(),
                      default_cluster_size(),
                      "blah");

        writeToVolume(*c1,
                      2 * default_cluster_multiplier(),
                      default_cluster_size(),
                      "blah");
    }

    writeToVolume(*c1,
                  default_cluster_multiplier(),
                  default_cluster_size(),
                  "joost");

    writeToVolume(*c1,
                  2 * default_cluster_multiplier(),
                  default_cluster_size(),
                  "wouter");

    const SnapshotName c1_snap1("c1_snap1");

    c1->createSnapshot(c1_snap1);
    waitForThisBackendWrite(*c1);

    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    //    const backend::Namespace ns2;
    const VolumeId clone2("clone2");

    SharedVolumePtr c2 = createClone(clone2,
                             ns2,
                             ns1,
                             c1_snap1);

    for(int i = 0; i < num_writes; ++i)
    {
        writeToVolume(*c2,
                      2 * default_cluster_multiplier(),
                      default_cluster_size(),
                      "fubar");
    }

    writeToVolume(*c2,
                  2 * default_cluster_multiplier(),
                  default_cluster_size(),
                  "wim");

    const SnapshotName c2_snap1("c2_snap1");

    c2->createSnapshot(c2_snap1);
    waitForThisBackendWrite(*c2);

    auto check_volumes([&]
    {
        checkVolume(*v1, 0, default_cluster_size(), "bart");
        checkVolume(*v1, default_cluster_multiplier(), default_cluster_size(), "arne");
        checkVolume(*v1, 2 * default_cluster_multiplier(), default_cluster_size(), "immanuel");
        checkVolume(*c1, 0, default_cluster_size(), "bart");
        checkVolume(*c1, default_cluster_multiplier(), default_cluster_size(), "joost");
        checkVolume(*c1, 2 * default_cluster_multiplier(), default_cluster_size(), "wouter");
        checkVolume(*c2, 0, default_cluster_size(), "bart");
        checkVolume(*c2, default_cluster_multiplier(), default_cluster_size(), "joost");
        checkVolume(*c2, 2 * default_cluster_multiplier(), default_cluster_size(), "wim");
    });

    auto check_consistency([&](Volume& v)
                           {
                               waitForThisBackendWrite(v);
                               bool res = true;
                               ASSERT_NO_THROW(res = v.checkConsistency());
                               EXPECT_TRUE(res);
                           });

    persistXVals(v1->getName());
    persistXVals(c1->getName());
    persistXVals(c2->getName());

    auto scrub_work_units = getScrubbingWork(vid);
    ASSERT_EQ(1U, scrub_work_units.size());

    scrubbing::ScrubReply scrub_result;

    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));

    ASSERT_NO_THROW(apply_scrubbing(clone1,
                                    scrub_result,
                                    ScrubbingCleanup::Never));

    check_volumes();

    check_consistency(*c1);

    ASSERT_NO_THROW(apply_scrubbing(clone2,
                                    scrub_result,
                                    ScrubbingCleanup::OnError));

    check_volumes();

    check_consistency(*c2);

    ASSERT_NO_THROW(apply_scrubbing(vid,
                                    scrub_result,
                                    ScrubbingCleanup::OnError));

    check_consistency(*v1);

    check_volumes();
    check_consistency(*v1);
    check_consistency(*c1);
    check_consistency(*c2);

    scrub_work_units = getScrubbingWork(clone1);
    ASSERT_EQ(1U, scrub_work_units.size());
    ASSERT_EQ(c1_snap1,
              scrub_work_units[0].snapshot_name_);

    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));
    ASSERT_NO_THROW(apply_scrubbing(clone2,
                                    scrub_result,
                                    ScrubbingCleanup::OnError));

    check_volumes();

    {
        waitForThisBackendWrite(*c2);
        ASSERT_NO_THROW(c2->checkConsistency());
    }

    ASSERT_NO_THROW(apply_scrubbing(clone1,
                                    scrub_result,
                                    ScrubbingCleanup::OnError));

    check_volumes();
    check_consistency(*v1);
    check_consistency(*c1);
    check_consistency(*c2);

    scrub_work_units = getScrubbingWork(clone2);
    ASSERT_EQ(1U, scrub_work_units.size());
    ASSERT_EQ(c2_snap1,
              scrub_work_units[0].snapshot_name_);

    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));
    ASSERT_NO_THROW(apply_scrubbing(clone2,
                                    scrub_result,
                                    ScrubbingCleanup::OnError));

    check_volumes();
    check_consistency(*v1);
    check_consistency(*c1);
    check_consistency(*c2);

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
    destroyVolume(c1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
    destroyVolume(c2,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(ScrubberTest, idempotent_scrub_result_application)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    const VolumeId vid("volume1");
    SharedVolumePtr v1 = newVolume(vid,
                           ns);

    const int num_writes = 1024;

    for(int i =0; i < num_writes; ++i)
    {
        writeToVolume(*v1, 0, default_cluster_size(), "xxx");
        writeToVolume(*v1, 8, default_cluster_size(), "xxx");
        writeToVolume(*v1, 16, default_cluster_size(), "xxx");
    }
    writeToVolume(*v1, 0, default_cluster_size(), "bart");
    writeToVolume(*v1, 8, default_cluster_size(), "arne");
    writeToVolume(*v1, 16, default_cluster_size(), "immanuel");

    const SnapshotName snap1("snap1");
    v1->createSnapshot(snap1);
    persistXVals(vid);
    waitForThisBackendWrite(*v1);

    const auto cns(make_random_namespace());
    const VolumeId cid(cns->ns().str());
    SharedVolumePtr c = createClone(cid,
                            cns->ns(),
                            ns_ptr->ns(),
                            snap1);

    ASSERT_TRUE(c != nullptr);

    const VolumeConfig volume_config = v1->get_config();

    auto scrub_work_units = getScrubbingWork(vid);
    ASSERT_EQ(1U, scrub_work_units.size());
    scrubbing::ScrubReply scrub_result;
    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));

    EXPECT_NO_THROW(apply_scrubbing(vid,
                                    scrub_result,
                                    ScrubbingCleanup::OnError,
                                    CollectScrubGarbage::F));

    waitForThisBackendWrite(*v1);

    EXPECT_NO_THROW(apply_scrubbing(cid,
                                    scrub_result,
                                    ScrubbingCleanup::OnError,
                                    CollectScrubGarbage::F));

    EXPECT_NO_THROW(apply_scrubbing(cid,
                                    scrub_result,
                                    ScrubbingCleanup::OnError,
                                    CollectScrubGarbage::F));

    EXPECT_NO_THROW(apply_scrubbing(vid,
                                    scrub_result,
                                    ScrubbingCleanup::Always,
                                    CollectScrubGarbage::T));

    EXPECT_THROW(apply_scrubbing(vid,
                                 scrub_result),
                 std::exception);

    waitForThisBackendWrite(*v1);

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    v1 = 0;

    ASSERT_NO_THROW(restartVolume(volume_config));
}

TEST_P(ScrubberTest, consistency)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    // backend::Namespace ns1;
    const VolumeId volume1("volume1");
    SharedVolumePtr v1 = newVolume(volume1,
			   ns1);
    const unsigned num_writes = 1;

    for(unsigned i =0; i < num_writes ; ++i)
    {
        writeToVolume(*v1, 0, default_cluster_size(), "xxx");
    }

    const SnapshotName v1_snap1("v1_snap1");
    v1->createSnapshot(v1_snap1);
    persistXVals(v1->getName());
    waitForThisBackendWrite(*v1);

    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();


    //    const backend::Namespace ns2;
    const VolumeId clone1("clone1");

    SharedVolumePtr c1 = createClone(clone1,
                             ns2,
                             ns1,
                             v1_snap1);

    persistXVals(volume1);
    auto scrub_work_units = getScrubbingWork(volume1);
    ASSERT_EQ(1U, scrub_work_units.size());
    ASSERT_EQ(v1_snap1,
              scrub_work_units[0].snapshot_name_);

    scrubbing::ScrubReply scrub_result;
    ASSERT_NO_THROW(scrub_result = do_scrub(scrub_work_units.front()));
    ASSERT_NO_THROW(apply_scrubbing(clone1,
                                    scrub_result,
                                    ScrubbingCleanup::OnError));

    ASSERT_NO_THROW(apply_scrubbing(volume1,
                                    scrub_result,
                                    ScrubbingCleanup::OnError));

    checkVolume(*c1,0,default_cluster_size(),"xxx");
    checkVolume(*v1,0,default_cluster_size(),"xxx");

    ASSERT_TRUE(c1->checkConsistency());
    ASSERT_TRUE(v1->checkConsistency());
    destroyVolume(c1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

// cf. OVS-3765
TEST_P(ScrubberTest, backend_error_while_fetching_relocations)
{
    auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    writeToVolume(*v,
                  0,
                  default_cluster_size(),
                  "0");

    const size_t overwrites = 2 * v->getSCOMultiplier();

    for (size_t i = 0; i < overwrites; ++i)
    {
        writeToVolume(*v,
                      v->getClusterMultiplier(),
                      default_cluster_size(),
                      boost::lexical_cast<std::string>(i + 1));
    }

    const SnapshotName snap("snap");
    v->createSnapshot(snap);
    waitForThisBackendWrite(*v);

    std::vector<scrubbing::ScrubWork> work(getScrubbingWork(v->getName()));
    ASSERT_EQ(1, work.size());

    const scrubbing::ScrubReply reply(do_scrub(work[0]));
    scrubbing::ScrubberResult res;

    be::BackendInterfacePtr bi(v->getBackendInterface()->clone());
    bi->fillObject<decltype(res),
                   boost::archive::text_iarchive>(res,
                                                  reply.scrub_result_name_,
                                                  InsistOnLatestVersion::T);

    ASSERT_FALSE(res.relocs.empty());
    bi->remove(res.relocs.back());

    EXPECT_THROW(v->applyScrubbingWork(reply),
                 TransientException);

    EXPECT_FALSE(v->is_halted());

    checkVolume(*v,
                0,
                default_cluster_size(),
                "0");

    checkVolume(*v,
                v->getClusterMultiplier(),
                default_cluster_size(),
                boost::lexical_cast<std::string>(overwrites));
}

namespace
{

const ClusterMultiplier
big_cluster_multiplier(VolManagerTestSetup::default_test_config().cluster_multiplier() * 2);

const auto big_clusters_config = VolManagerTestSetup::default_test_config()
    .cluster_multiplier(big_cluster_multiplier);

}

INSTANTIATE_TEST_CASE_P(ScrubberTests,
                        ScrubberTest,
                        ::testing::Values(volumedriver::VolManagerTestSetup::default_test_config(),
                                          big_clusters_config));

}

// Local Variables: **
// mode: c++ **
// End: **
