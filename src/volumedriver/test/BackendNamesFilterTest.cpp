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

#include <boost/random/uniform_int_distribution.hpp>

#include <backend/RestLockCommunicator.h>

#include <volumedriver/BackendNamesFilter.h>
#include <volumedriver/FailOverCacheConfig.h>
#include <volumedriver/SCOAccessData.h>
#include <volumedriver/SnapshotManagement.h>
#include <volumedriver/VolumeConfig.h>

namespace volumedrivertest
{

using namespace volumedriver;
namespace be = backend;

class BackendNamesFilterTest
    : public ExGTest
{
protected:
    void
    test(const std::string& s)
    {
        EXPECT_TRUE(f_(s)) << s;
        EXPECT_FALSE(f_(s + '0')) << s;
        EXPECT_FALSE(f_(s + '1')) << s;
        EXPECT_FALSE(f_(s + 'A')) << s;
        EXPECT_FALSE(f_(s + 'a')) << s;
        EXPECT_FALSE(f_(s + 'F')) << s;
        EXPECT_FALSE(f_(s + 'f')) << s;
        EXPECT_FALSE(f_(s.substr(1))) << s;
        if (s.size() > 2)
        {
            EXPECT_FALSE(f_(s.substr(0, s.size() - 2))) << s;
        }
    }

private:
    BackendNamesFilter f_;
};

TEST_F(BackendNamesFilterTest, tlogs)
{
    const int ntlogs = 1000;
    for (auto i = 0; i < ntlogs; ++i)
    {
        const std::string s(TLog::getName(TLogID()));
        test(s);
    }
}

TEST_F(BackendNamesFilterTest, scos)
{
    const int nscos = 1000;
    for (auto i = 0; i < nscos; ++i)
    {
        const SCONumber n = 1 + (std::numeric_limits<SCONumber>::max() - 1) * drand48();
        const SCOVersion v = SCOVersion(std::numeric_limits<SCOVersion>::max() * drand48());
        const SCOCloneID c = SCOCloneID(std::numeric_limits<SCOCloneID>::max() * drand48());

        SCO sco(n, c, v);
        test(sco.str());
    }
}

TEST_F(BackendNamesFilterTest, configs_etc)
{
    test(SCOAccessDataPersistor::backend_name);
    test(FailOverCacheConfig::config_backend_name);
    test(VolumeConfig::config_backend_name);
    test(SnapshotManagement::getSnapshotFilename());
}

TEST_F(BackendNamesFilterTest, things_that_must_not_match)
{
    BackendNamesFilter f;
    EXPECT_FALSE(f("replicationMetaInitialized"));
    EXPECT_FALSE(f("replicationConfig"));
    EXPECT_FALSE(f(be::rest::RestLockCommunicator::lock_name_));
}

TEST_F(BackendNamesFilterTest, random)
{
    // anything I forgot?
    static const std::string chars("abcdefghijklmnopqrstuvwxyz"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "1234567890"
                                   "!@#$%^&*()"
                                   "`~-_=+[]{}\\|;:'\",<.>/? ");

    boost::mt19937 rng;
    boost::random::uniform_int_distribution<> index_dist(0, chars.size() - 1);
    boost::random::uniform_int_distribution<> size_dist(0, 255);

    BackendNamesFilter f;

    const int ntests = 10000;

    for (auto i = 0; i < ntests; ++i)
    {
        std::stringstream ss;
        for (auto j = 0; j < size_dist(rng); ++j)
        {
            ss << chars[index_dist(rng)];
        }

        const std::string s = ss.str();
        if (s != SCOAccessDataPersistor::backend_name and
            s != FailOverCacheConfig::config_backend_name and
            s != VolumeConfig::config_backend_name and
            s != SnapshotManagement::getSnapshotFilename() and
            not TLog::isTLogString(s) and
            not SCO::isSCOString(s))
        {
            EXPECT_FALSE(f(s)) << s;
        }
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
