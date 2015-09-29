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

class VolumeStateManagementTest
    : public VolManagerTestSetup
{
public:
    VolumeStateManagementTest()
        : VolManagerTestSetup("VolumeStateManagementTest")
    {}
};

TEST_P(VolumeStateManagementTest, test1)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume("vol1",
			  ns1);

    ASSERT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);

    writeToVolume(v,
                  0,
                  4096,
                  "xyz");

    checkVolume(v,
                0,
                4096,
                "xyz");

    ASSERT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, NoCache)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume("vol1",
			  ns1);
    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                         FailOverCacheTestSetup::port_base())),
                 fungi::IOException);

    ASSERT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);

    writeToVolume(v,
                  0,
                  4096,
                  "xyz");

    checkVolume(v,
                0,
                4096,
                "xyz");

    v->setFailOverCacheConfig(boost::none);

    ASSERT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, test2)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume("vol1",
			  ns1);

    {
        auto foc_ctx(start_one_foc());

        v->setFailOverCacheConfig(foc_ctx->config());

        ASSERT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_SYNC);
        for(int i =0; i < 32; ++i)
        {
            writeToVolume(v,
                          0,
                          4096,
                          "xyz");
        }
    }

    for(int i =0; i < 32; ++i)
    {
        writeToVolume(v,
                      0,
                      4096,
                      "abc");
    }

    flushFailOverCache(v);

    ASSERT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);
    checkVolume(v,
                0,
                4096,
                "abc");

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
}

TEST_P(VolumeStateManagementTest, CreateVolumeWithRemoteCache)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume("vol1",
			  ns1);

    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                         FailOverCacheTestSetup::port_base())),
                 fungi::IOException);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);

    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                         FailOverCacheTestSetup::port_base())),
                 fungi::IOException);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);
    v->setFailOverCacheConfig(boost::none);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);

    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                         FailOverCacheTestSetup::port_base())),
                 fungi::IOException);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);

    {
        auto foc_ctx(start_one_foc());

        {
            SCOPED_BLOCK_BACKEND(v);

            writeToVolume(v,0,4096,"bart");
            EXPECT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));
            EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::KETCHUP);
        }

        waitForThisBackendWrite(v);
        EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_SYNC);
    }

    // Volume might take some time to find out it's failover is not there anymore.
    sleep(2);
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);
    v->setFailOverCacheConfig(boost::none);
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);
}

TEST_P(VolumeStateManagementTest, CreateVolumeStandalone)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    Volume* v = newVolume("vol1",
			  ns1);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);
    EXPECT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                         FailOverCacheTestSetup::port_base())),
                 fungi::IOException);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);
    v->setFailOverCacheConfig(boost::none);
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);

    {
        auto foc_ctx(start_one_foc());

        {
            SCOPED_BLOCK_BACKEND(v);

            EXPECT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));
            EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_SYNC);
        }
    }

    flushFailOverCache(v);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);
    v->setFailOverCacheConfig(boost::none);
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);
}

TEST_P(VolumeStateManagementTest, CreateVolumeStandaloneLocalRestart)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;

    Volume* v = newVolume("vol1",
			  ns1);

    writeToVolume (v, 0, 4096, "bart");
    v->sync();

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);
    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v = 0;

    ASSERT_NO_THROW(v = localRestart(ns1));

    checkVolume(v,0,4096, "bart");
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, CreateVolumeWithFailoverLocalRestart)
{
    auto foc_ctx(start_one_foc());
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    Volume* v = newVolume("vol1",
                          ns1);
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);

    ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

    writeToVolume (v, 0, 4096, "bart");
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_SYNC);
    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    ASSERT_NO_THROW(v = localRestart(ns1));

    checkVolume(v,0,4096, "bart");
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_SYNC);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, CreateVolumeWithFailoverLocalRestart2)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    Volume* v = newVolume("vol1",
			  ns1);
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);

    {
        auto foc_ctx(start_one_foc());

        ASSERT_NO_THROW(v->setFailOverCacheConfig(foc_ctx->config()));

        writeToVolume (v, 0, 4096, "bart");
        EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_SYNC);
    }

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    ASSERT_NO_THROW(v = localRestart(ns1));

    checkVolume(v,0,4096, "bart");
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

void checkReachesState(Volume* v, VolumeFailoverState s, int maxTime = 3)
{
    int cnt = 0;
    while (cnt++ < maxTime)
    {
        if (v->getVolumeFailoverState() == s) return;
        sleep(1);
    }
    EXPECT_TRUE(s == v->getVolumeFailoverState());
}

TEST_P(VolumeStateManagementTest, CreateVolumeStandaloneLocalRestartWithFailOver1)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    Volume* v = newVolume("vol1",
			  ns1);

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,1,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        writeToVolume (v, 0, 4096, "bart");
    }

    auto foc_ctx(start_one_foc());

    {
        SCOPED_BLOCK_BACKEND(v);

        ASSERT_NO_THROW(v = localRestart(ns1));
        v->setFailOverCacheConfig(foc_ctx->config());

        checkVolume(v,0,4096, "bart");
    }
    checkReachesState(v, VolumeFailoverState::OK_SYNC, 20);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}


TEST_P(VolumeStateManagementTest, CreateVolumeStandaloneLocalRestartWithFailOver2)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    Volume* v = newVolume("vol1",
                          ns1);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);
    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    auto foc_ctx(start_one_foc());

    {
        SCOPED_BLOCK_BACKEND(v);

        ASSERT_NO_THROW(v = localRestart(ns1));
        v->setFailOverCacheConfig(foc_ctx->config());

        EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_SYNC);
    }

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}


TEST_P(VolumeStateManagementTest, CreateVolumeStandaloneLocalRestartWithFailOver3)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    Volume* v = newVolume("vol1",
			  ns1);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_STANDALONE);
    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    ASSERT_NO_THROW(v = localRestart(ns1));

    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                         FailOverCacheTestSetup::port_base())),
                 fungi::IOException);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, CreateVolumeNonLocalRestart1)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    Volume* v = newVolume("vol1",
			  ns1);

    ASSERT_THROW(v->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                         FailOverCacheTestSetup::port_base())),
                 fungi::IOException);

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);
    waitForThisBackendWrite(v);

    VolumeConfig cfg = v->get_config();
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
    EXPECT_THROW(restartVolume(cfg),
                 fungi::IOException);
    EXPECT_NO_THROW(restartVolume(cfg,
                               PrefetchVolumeData::F,
                               CheckVolumeNameForRestart::T,
                               IgnoreFOCIfUnreachable::T));


    v= getVolume(VolumeId("vol1"));

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, CreateVolumeNonLocalRestart2)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    Volume* v = newVolume("vol1",
                          ns1);

    {
        auto foc_ctx(start_one_foc());
        v->setFailOverCacheConfig(foc_ctx->config());

        EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_SYNC);
        waitForThisBackendWrite(v);

        VolumeConfig cfg = v->get_config();
        destroyVolume(v,
                      DeleteLocalData::T,
                      RemoveVolumeCompletely::F);
        restartVolume(cfg);
        v= getVolume(VolumeId("vol1"));
        EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_SYNC);
    }

    sleep(2);
    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::DEGRADED);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, CreateVolumeNonLocalRestart3)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    Volume* v = newVolume("vol1",
			  ns1);

    auto foc_ctx(start_one_foc());
    v->setFailOverCacheConfig(foc_ctx->config());

    EXPECT_TRUE(v->getVolumeFailoverState() == VolumeFailoverState::OK_SYNC);
    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);

    VolumeConfig cfg = v->get_config();

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    foc_ctx.reset();

    EXPECT_THROW(restartVolume(cfg),
                 fungi::IOException);
}

INSTANTIATE_TEST(VolumeStateManagementTest);

}

// Local Variables: **
// mode: c++ **
// End: **
