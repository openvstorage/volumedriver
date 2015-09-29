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
    const std::string snapshot_name_ =  { "a_snapshot_name" };
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
    VolumeId id("a_volume_id");
    backend::Namespace ns;
    std::string scrub_result_name;

    ScrubReply reply(id,
                     ns,
                     scrub_result_name);

    std::string serialized = reply.str();

    ScrubReply reply2(serialized);

    ASSERT_TRUE(reply2.id_ == id);
    ASSERT_TRUE(reply2.ns_ == ns);
    ASSERT_TRUE(reply2.scrub_result_name_ == scrub_result_name);
}

}
