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
    SetUp() override
    {
        const std::string s(testing::UnitTest::GetInstance()->current_test_info()->test_case_name());
        pathPfx_ = getTempPath(s);
        fs::remove_all(pathPfx_);
        fs::create_directories(pathPfx_);
        initialize_connection_manager();

    }

    virtual void
    TearDown() override
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
