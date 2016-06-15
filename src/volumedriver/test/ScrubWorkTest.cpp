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

#include "ExGTest.h"
#include "../ScrubWork.h"
#include "../ScrubReply.h"

#include <youtils/Assert.h>

#include <backend/AlbaConfig.h>
#include <backend/LocalConfig.h>
#include <backend/S3Config.h>

namespace volumedrivertest
{
using namespace volumedriver;
using namespace scrubbing;

TODO("Y42 MAKE A TEST THAT FAILS IF YOU ADD A NEW BACKEND");

class ScrubWorkTest
    : public ExGTest
{
public:
    ScrubWork
    make_scrubwork(const backend::BackendConfig& be)
    {
        return ScrubWork(be.clone(),
                         ns_,
                         vid_,
                         cluster_exponent_,
                         sco_size_,
                         snapshot_name_);
    }

    void
    check_scrub_work(const backend::BackendConfig& be,
                     const ScrubWork& scrubwork)
    {
        ASSERT_TRUE(static_cast<bool>(scrubwork.backend_config_));
        ASSERT_TRUE(be == *scrubwork.backend_config_);
        ASSERT_TRUE(scrubwork.ns_ == ns_);
        ASSERT_TRUE(scrubwork.id_ == vid_);
        ASSERT_TRUE(scrubwork.cluster_exponent_ == cluster_exponent_);
        ASSERT_TRUE(scrubwork.sco_size_ == sco_size_);
        ASSERT_TRUE(scrubwork.snapshot_name_ == snapshot_name_);
    }

    const Namespace ns_ = { std::string("a-namespace") };
    const VolumeId vid_ = VolumeId("a_volume_id");
    const SnapshotName snapshot_name_ = SnapshotName("a_snapshot_name");
    const volumedriver::ClusterExponent cluster_exponent_ = 25;
    const uint32_t sco_size_ = 3;
};

TEST_F(ScrubWorkTest, serialization_s3)
{
    backend::S3Config s3_config(backend::S3Flavour::S3,
                                "a_host",
                                80,
                                "a_username",
                                "a_password",
                                true,
                                backend::UseSSL::F,
                                backend::SSLVerifyHost::T,
                                "");

    ScrubWork t1 =  make_scrubwork(s3_config);

    std::stringstream oss;
    ScrubWork::oarchive_type oa(oss);

    ASSERT_NO_THROW(oa & BOOST_SERIALIZATION_NVP(t1));

    std::stringstream iss(oss.str());

    ScrubWork::iarchive_type ia(iss);

    ScrubWork t2;
    ASSERT_NO_THROW(ia & BOOST_SERIALIZATION_NVP(t2));
    check_scrub_work(s3_config,
                     t2);
}

TEST_F(ScrubWorkTest, serialization1_local)
{
    backend::LocalConfig config("a_path",
                       0,
                       0);

    ScrubWork t1 =  make_scrubwork(config);

    std::stringstream oss;
    ScrubWork::oarchive_type oa(oss);

    ASSERT_NO_THROW(oa & BOOST_SERIALIZATION_NVP(t1));

    std::stringstream iss(oss.str());

    ScrubWork::iarchive_type ia(iss);

    ScrubWork t2;
    ASSERT_NO_THROW(ia & BOOST_SERIALIZATION_NVP(t2));
    check_scrub_work(config,
                     t2);
}

TEST_F(ScrubWorkTest, serialization1_alba)
{
    const backend::AlbaConfig config("localhost",
                                     12345,
                                     60);

    ScrubWork t1 = make_scrubwork(config);

    std::stringstream oss;
    ScrubWork::oarchive_type oa(oss);

    ASSERT_NO_THROW(oa & BOOST_SERIALIZATION_NVP(t1));

    std::stringstream iss(oss.str());

    ScrubWork::iarchive_type ia(iss);

    ScrubWork t2;
    ASSERT_NO_THROW(ia & BOOST_SERIALIZATION_NVP(t2));
    check_scrub_work(config,
                     t2);
}

TEST_F(ScrubWorkTest, serialization_of_scrub_reply)
{
    const backend::Namespace ns;
    const std::string scrub_result_name("some-result");
    const SnapshotName snap_name("some-snapshot");

    ScrubReply reply(ns,
                     snap_name,
                     scrub_result_name);

    std::string serialized = reply.str();

    ScrubReply reply2(serialized);

    ASSERT_EQ(ns, reply2.ns_);
    ASSERT_EQ(snap_name,
              reply2.snapshot_name_);
    ASSERT_EQ(scrub_result_name,
              reply2.scrub_result_name_);
}

TEST_F(ScrubWorkTest, scrub_reply_equality)
{
    const backend::Namespace ns1;
    const std::string res1("scrub_result_1");
    const SnapshotName snap1("some-snapshot-1");

    const ScrubReply rep1(ns1,
                          snap1,
                          res1);

    EXPECT_EQ(rep1,
              ScrubReply(ns1,
                         snap1,
                         res1));

    const backend::Namespace ns2;
    const std::string res2("scrub_result_2");
    const SnapshotName snap2("some-snapshot-2");

    EXPECT_NE(rep1,
              ScrubReply(ns2,
                         snap2,
                         res1));

    EXPECT_NE(rep1,
              ScrubReply(ns1,
                         snap2,
                         res2));

    EXPECT_NE(rep1,
              ScrubReply(ns2,
                         snap2,
                         res2));
}

TEST_F(ScrubWorkTest, scrub_reply_order)
{
    const backend::Namespace ns1;
    const backend::Namespace ns2;

    const SnapshotName snap1("snap1");
    const std::string res1("scrub_result_1");

    const ScrubReply rep1(ns1,
                          snap1,
                          res1);

    ASSERT_NE(ns1, ns2);

    if (ns1 < ns2)
    {
        EXPECT_LT(rep1,
                  ScrubReply(ns2,
                             snap1,
                             res1));
        EXPECT_LT(rep1,
                  ScrubReply(ns2,
                             SnapshotName("snap0"),
                             "scrub_result_0"));
    }
    else
    {
        EXPECT_LT(ScrubReply(ns2,
                             snap1,
                             res1),
                  rep1);
        EXPECT_LT(ScrubReply(ns2,
                             SnapshotName("snap2"),
                             "scrub_result_2"),
                  rep1);
    }
}

}
