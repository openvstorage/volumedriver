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

#ifndef SCO_CACHE_TEST_SETUP_H_
#define SCO_CACHE_TEST_SETUP_H_

#include <youtils/Logging.h>

#include <boost/foreach.hpp>

#include "ExGTest.h"
#include <backend/BackendTestSetup.h>
#include "../SCOCache.h"

namespace volumedriver
{
namespace be = backend;

class SCOCacheTestSetup
    : public ExGTest
    , public be::BackendTestSetup

{
protected:
    SCOCacheTestSetup()
        : ExGTest()
        , be::BackendTestSetup()
    {}

    virtual void
    SetUp()
    {
        const std::string s(testing::UnitTest::GetInstance()->current_test_info()->test_case_name());
        pathPfx_ = getTempPath(s);
        fs::remove_all(pathPfx_);
        fs::create_directories(pathPfx_);
        initialize_connection_manager();

    }

    virtual void
    TearDown()
    {
        uninitialize_connection_manager();
        fs::remove_all(pathPfx_);
    }

    typedef std::list<SCOCacheMountPointPtr> SCOCacheMountPointList;

    SCOCacheMountPointList&
    getMountPointList(SCOCache& scoCache)
    {
        return scoCache.mountPoints_;
    }

    uint64_t
    getMountPointErrorCount(SCOCache& scocache)
    {
        return scocache.mpErrorCount_;
    }

    typedef std::map<const backend::Namespace, SCOCacheNamespace*> NSMap;

    NSMap&
    getNSMap(SCOCache& scoCache)
    {
        return scoCache.nsMap_;
    }

    fs::path pathPfx_;
};

}

#endif

// Local Variables: **
// mode: c++ **
// End: **
