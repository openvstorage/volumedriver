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

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cassert>
// Y42 Make hell freeze over by the end of the week.
#ifdef HELL_FREEZES_OVER
#include "ExGTest.h"

#include "TCBTMetaDataStoreSim.h"
#include "MetaDataStoreInterface.h"

namespace volumedriver {

class TCBTMetaDataStoreSimTest : public ExGTest
{
protected:
    void SetUp()
    {
        std::string testCase(testing::UnitTest::GetInstance()->current_test_info()->test_case_name());
	pathPfx_ = getTempPath(testCase);
        removeTestDir(pathPfx_);
        fungi::File::recursiveMkDir("/", pathPfx_);
        md_.reset(new TCBTMetaDataStoreSim<14>(pathPfx_, 0));
    }

    void TearDown()
    {
        md_.reset(0);
        removeTestDir(pathPfx_);
    }

    void removeTestDir(const std::string &dir)
    {
	fs::remove_all(dir);
    }

    void logStatus()
    {
        std::ifstream f("/proc/self/status");
        char buf[4096] = {0};

        while (f.good()) {
            f.getline(buf, sizeof(buf));
            std::cerr << buf << std::endl;
            //LOG_INFO("" << buf);
        }
        f.close();
    }

    DECLARE_LOGGER("TCBTMetaDataStoreSimTest");
    std::auto_ptr<MetaDataStoreInterface> md_;
    std::string pathPfx_;
};

TEST_F(TCBTMetaDataStoreSimTest, DISABLED_fill)
{
    const char *entry_env = getenv("TCBT_ENTRIES");
    uint64_t entries = entry_env ? strtoull(entry_env, NULL, 0) : 1 << 20;
    ClusterLocation l(1, 1, 0, 0);

    //LOG_INFO("Adding " << entries << " entries");
    std::cerr << "=== Adding " << entries << " entries ===" << std::endl;
    logStatus();
    for (uint64_t i = 0; i < entries; ++i) {
        md_->writeCluster(i, l);
    }

    //LOG_INFO("Added " << entries << " entries");
    std::cerr << "=== Added " << entries << " entries ===" << std::endl;
    logStatus();
}

}
#endif
// Local Variables: **
// mode: c++ **
// End: **
