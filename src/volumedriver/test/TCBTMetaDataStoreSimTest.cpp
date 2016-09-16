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

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cassert>
// Y42 Make hell freeze over by the end of the week.
#ifdef HELL_FREEZES_OVER
#include "VolumeDriverTestConfig.h"

#include "TCBTMetaDataStoreSim.h"
#include "MetaDataStoreInterface.h"

namespace volumedriver {

class TCBTMetaDataStoreSimTest : public testing::TestWithParam<VolumeDriverTestConfig>
{
protected:
    void SetUp()
    {
        std::string testCase(testing::UnitTest::GetInstance()->current_test_info()->test_case_name());
	pathPfx_ = yt::FileUtils::temp_path(testCase);
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
