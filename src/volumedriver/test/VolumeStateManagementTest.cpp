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

    SharedVolumePtr v = newVolume("vol1",
			  ns1);

    ASSERT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());

    writeToVolume(*v,
                  0,
                  4096,
                  "xyz");

    checkVolume(*v,
                0,
                4096,
                "xyz");

    ASSERT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, NoCache)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v = newVolume("vol1",
			  ns1);
    ASSERT_THROW(v->setDtlConfig(DtlConfig(DtlTestSetup::host(),
                                                               DtlTestSetup::port_base(),
                                                               GetParam().foc_mode())),
                 fungi::IOException);

    ASSERT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    writeToVolume(*v,
                  0,
                  4096,
                  "xyz");

    checkVolume(*v,
                0,
                4096,
                "xyz");

    v->setDtlConfig(boost::none);

    ASSERT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, test2)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v = newVolume("vol1",
			  ns1);

    {
        auto foc_ctx(start_one_foc());

        v->setDtlConfig(foc_ctx->config(GetParam().foc_mode()));

        ASSERT_EQ(VolumeFailOverState::OK_SYNC,
                  v->getVolumeFailOverState());

        for(int i =0; i < 32; ++i)
        {
            writeToVolume(*v,
                          0,
                          4096,
                          "xyz");
        }
    }

    for(int i = 0; i < 32; ++i)
    {
        writeToVolume(*v,
                      0,
                      4096,
                      "abc");
    }

    flushFailOverCache(*v);

    ASSERT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    checkVolume(*v,
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

    SharedVolumePtr v = newVolume("vol1",
			  ns1);

    ASSERT_THROW(v->setDtlConfig(DtlConfig(DtlTestSetup::host(),
                                                               DtlTestSetup::port_base(),
                                                               GetParam().foc_mode())),
                 fungi::IOException);

    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    ASSERT_THROW(v->setDtlConfig(DtlConfig(DtlTestSetup::host(),
                                                               DtlTestSetup::port_base(),
                                                               GetParam().foc_mode())),
                 fungi::IOException);

    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    v->setDtlConfig(boost::none);

    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());

    ASSERT_THROW(v->setDtlConfig(DtlConfig(DtlTestSetup::host(),
                                                               DtlTestSetup::port_base(),
                                                               GetParam().foc_mode())),
                 fungi::IOException);

    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    {
        auto foc_ctx(start_one_foc());

        {
            SCOPED_BLOCK_BACKEND(*v);

            writeToVolume(*v,0,4096,"bart");
            EXPECT_NO_THROW(v->setDtlConfig(foc_ctx->config(GetParam().foc_mode())));
            EXPECT_EQ(VolumeFailOverState::KETCHUP,
                      v->getVolumeFailOverState());
        }

        waitForThisBackendWrite(*v);
        EXPECT_EQ(VolumeFailOverState::OK_SYNC,
                  v->getVolumeFailOverState());
    }

    flushFailOverCache(*v); // It is only detected for sure that the FOC is gone when something happens over the wire

    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    v->setDtlConfig(boost::none);
    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());
}

TEST_P(VolumeStateManagementTest, CreateVolumeStandalone)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    SharedVolumePtr v = newVolume("vol1",
			  ns1);

    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());
    EXPECT_THROW(v->setDtlConfig(DtlConfig(DtlTestSetup::host(),
                                                               DtlTestSetup::port_base(),
                                                               GetParam().foc_mode())),
                 fungi::IOException);

    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    v->setDtlConfig(boost::none);
    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());

    {
        auto foc_ctx(start_one_foc());

        {
            SCOPED_BLOCK_BACKEND(*v);

            EXPECT_NO_THROW(v->setDtlConfig(foc_ctx->config(GetParam().foc_mode())));
            EXPECT_EQ(VolumeFailOverState::OK_SYNC,
                      v->getVolumeFailOverState());
        }
    }

    flushFailOverCache(*v);

    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());
    v->setDtlConfig(boost::none);
    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());
}

TEST_P(VolumeStateManagementTest, CreateVolumeStandaloneLocalRestart)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;

    SharedVolumePtr v = newVolume("vol1",
			  ns1);

    writeToVolume (*v, 0, 4096, "bart");
    v->sync();

    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());
    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    v = 0;

    ASSERT_NO_THROW(v = localRestart(ns1));

    checkVolume(*v,0,4096, "bart");
    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());
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
    SharedVolumePtr v = newVolume("vol1",
                          ns1);
    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());

    ASSERT_NO_THROW(v->setDtlConfig(foc_ctx->config(GetParam().foc_mode())));

    writeToVolume(*v, 0, 4096, "bart");
    EXPECT_EQ(VolumeFailOverState::OK_SYNC,
              v->getVolumeFailOverState());
    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    ASSERT_NO_THROW(v = localRestart(ns1));

    checkVolume(*v,0,4096, "bart");
    EXPECT_EQ(VolumeFailOverState::OK_SYNC,
              v->getVolumeFailOverState());
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, CreateVolumeWithFailoverLocalRestart2)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    SharedVolumePtr v = newVolume("vol1",
			  ns1);
    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());

    {
        auto foc_ctx(start_one_foc());

        ASSERT_NO_THROW(v->setDtlConfig(foc_ctx->config(GetParam().foc_mode())));

        writeToVolume(*v, 0, 4096, "bart");
        EXPECT_EQ(VolumeFailOverState::OK_SYNC,
                  v->getVolumeFailOverState());
    }

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    ASSERT_NO_THROW(v = localRestart(ns1));

    checkVolume(*v,0,4096, "bart");
    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

namespace
{

void checkReachesState(SharedVolumePtr v, VolumeFailOverState s, int maxTime = 3)
{
    int cnt = 0;
    while (cnt++ < maxTime)
    {
        if (v->getVolumeFailOverState() == s) return;
        sleep(1);
    }
    EXPECT_EQ(s, v->getVolumeFailOverState());
}

}

TEST_P(VolumeStateManagementTest, DISABLED_CreateVolumeStandaloneLocalRestartWithFailOver1)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    SharedVolumePtr v = newVolume("vol1",
			  ns1);

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v,1,
                                              DeleteLocalData::F,
                                              RemoveVolumeCompletely::F);
        writeToVolume(*v, 0, 4096, "bart");
    }
    TODO("AR: how is this supposed to work - v is destroyed and afterwards the backend queue shall be blocked?")
    auto foc_ctx(start_one_foc());

    {
        SCOPED_BLOCK_BACKEND(*v);
        v = nullptr;

        ASSERT_NO_THROW(v = localRestart(ns1));
        v->setDtlConfig(foc_ctx->config(GetParam().foc_mode()));

        checkVolume(*v,0,4096, "bart");
    }
    checkReachesState(v, VolumeFailOverState::OK_SYNC, 20);
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TODO("AR: this one breaks now that we use shared/weak ptrs")
TEST_P(VolumeStateManagementTest, DISABLED_CreateVolumeStandaloneLocalRestartWithFailOver2)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    SharedVolumePtr v = newVolume("vol1",
                          ns1);

    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());

    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F); // sets 'v' to nullptr

    auto foc_ctx(start_one_foc());

    {
        SCOPED_BLOCK_BACKEND(*v); // <-- nullptr deref.

        ASSERT_NO_THROW(v = localRestart(ns1));
        v->setDtlConfig(foc_ctx->config(GetParam().foc_mode()));

        EXPECT_EQ(VolumeFailOverState::OK_SYNC,
                  v->getVolumeFailOverState());
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
    SharedVolumePtr v = newVolume("vol1",
			  ns1);

    EXPECT_EQ(VolumeFailOverState::OK_STANDALONE,
              v->getVolumeFailOverState());
    destroyVolume(v,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);
    ASSERT_NO_THROW(v = localRestart(ns1));

    ASSERT_THROW(v->setDtlConfig(DtlConfig(DtlTestSetup::host(),
                                                               DtlTestSetup::port_base(),
                                                               GetParam().foc_mode())),
                 fungi::IOException);

    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, CreateVolumeNonLocalRestart1)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    SharedVolumePtr v = newVolume("vol1",
			  ns1);

    ASSERT_THROW(v->setDtlConfig(DtlConfig(DtlTestSetup::host(),
                                                               DtlTestSetup::port_base(),
                                                               GetParam().foc_mode())),
                 fungi::IOException);

    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    waitForThisBackendWrite(*v);

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

    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, CreateVolumeNonLocalRestart2)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    SharedVolumePtr v = newVolume("vol1",
                          ns1);

    {
        auto foc_ctx(start_one_foc());
        v->setDtlConfig(foc_ctx->config(GetParam().foc_mode()));

        EXPECT_EQ(VolumeFailOverState::OK_SYNC,
                  v->getVolumeFailOverState());
        waitForThisBackendWrite(*v);

        VolumeConfig cfg = v->get_config();
        destroyVolume(v,
                      DeleteLocalData::T,
                      RemoveVolumeCompletely::F);
        restartVolume(cfg);
        v= getVolume(VolumeId("vol1"));
        EXPECT_EQ(VolumeFailOverState::OK_SYNC,
                  v->getVolumeFailOverState());
    }

    flushFailOverCache(*v); // It is only detected for sure that the FOC is gone when something happens over the wire

    EXPECT_EQ(VolumeFailOverState::DEGRADED,
              v->getVolumeFailOverState());

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(VolumeStateManagementTest, CreateVolumeNonLocalRestart3)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();

    // backend::Namespace ns1;
    SharedVolumePtr v = newVolume("vol1",
			  ns1);

    auto foc_ctx(start_one_foc());
    v->setDtlConfig(foc_ctx->config(GetParam().foc_mode()));

    EXPECT_EQ(VolumeFailOverState::OK_SYNC,
              v->getVolumeFailOverState());

    waitForThisBackendWrite(*v);
    waitForThisBackendWrite(*v);

    VolumeConfig cfg = v->get_config();

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    foc_ctx.reset();

    EXPECT_THROW(restartVolume(cfg),
                 fungi::IOException);
}

TEST_P(VolumeStateManagementTest, events)
{
    auto wrns(make_random_namespace());
    SharedVolumePtr v = newVolume(*wrns);

    auto check_ev([&](events::DTLState old_state,
                      events::DTLState new_state)
                  {
                      const boost::optional<events::Event> ev(event_collector_->pop());

                      ASSERT_TRUE(boost::none != ev);

                      ASSERT_TRUE(ev->HasExtension(events::dtl_state_transition));

                      const events::DTLStateTransitionEvent&
                          trans(ev->GetExtension(events::dtl_state_transition));

                      ASSERT_TRUE(trans.has_volume_name());
                      EXPECT_EQ(v->getName().str(),
                                trans.volume_name());

                      ASSERT_TRUE(trans.has_old_state());
                      EXPECT_EQ(old_state,
                                trans.old_state());

                      ASSERT_TRUE(trans.has_new_state());
                      EXPECT_EQ(new_state,
                                trans.new_state());
                  });

    ASSERT_EQ(1U,
              event_collector_->size());

    check_ev(events::DTLState::Degraded,
             events::DTLState::Standalone);

    const std::string s("some data");

    writeToVolume(*v,
                  0,
                  4096,
                  s);

    {
        auto foc_ctx(start_one_foc());
        v->setDtlConfig(foc_ctx->config(GetParam().foc_mode()));

        ASSERT_EQ(2U,
                  event_collector_->size());

        check_ev(events::DTLState::Standalone,
                 events::DTLState::Degraded);

        check_ev(events::DTLState::Degraded,
                 events::DTLState::Catchup);

        v->scheduleBackendSync();
        waitForThisBackendWrite(*v);

        ASSERT_EQ(1U,
                  event_collector_->size());

        check_ev(events::DTLState::Catchup,
                 events::DTLState::Sync);
    }

    v->sync();

    ASSERT_EQ(1U,
              event_collector_->size());

    check_ev(events::DTLState::Sync,
             events::DTLState::Degraded);

    v->setDtlConfig(boost::none);

    ASSERT_EQ(1U,
              event_collector_->size());

    check_ev(events::DTLState::Degraded,
             events::DTLState::Standalone);
}

namespace
{

const VolumeDriverTestConfig cluster_cache_config =
    VolumeDriverTestConfig().use_cluster_cache(true);

const VolumeDriverTestConfig sync_foc_config =
    VolumeDriverTestConfig()
    .use_cluster_cache(true)
    .foc_mode(DtlMode::Synchronous);

}

INSTANTIATE_TEST_CASE_P(VolumeStateManagementTests,
                        VolumeStateManagementTest,
                        ::testing::Values(cluster_cache_config,
                                          sync_foc_config));
}

// Local Variables: **
// mode: c++ **
// End: **
