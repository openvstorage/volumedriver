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

#ifndef EXGTEST_H_
#define EXGTEST_H_

#include "../FailOverCacheMode.h"
#include "../Types.h"
#include "../VolumeConfig.h"

#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <gtest/gtest.h>

#include <youtils/IOException.h>

namespace volumedriver
{

struct VolumeDriverTestConfig
{
#define PARAM(type, name)                                       \
                                                                \
    const type&                                                 \
    name() const                                                \
    {                                                           \
        return name ## _;                                       \
    }                                                           \
                                                                \
    VolumeDriverTestConfig&                                     \
    name(const type& val)                                       \
    {                                                           \
        name ## _ = val;                                        \
        return *this;                                           \
    }                                                           \
                                                                \
    type name ## _

    PARAM(bool, use_cluster_cache) = false;
    PARAM(bool, foc_in_memory) = false;
    PARAM(FailOverCacheMode, foc_mode) = FailOverCacheMode::Asynchronous;
    PARAM(ClusterMultiplier, cluster_multiplier) =
        VolumeConfig::default_cluster_multiplier();

#undef PARAM
};

// TODO: this is not the best approach; please remake sometime by overriding the calling of the testbody
// directly, which will require some more subtle changes to google test

// Z42: rename ExGTest to VolumeDriverTest
class ExGTest :
        public testing::TestWithParam<VolumeDriverTestConfig>
{
protected:
    ExGTest()
        : catch_(true)
    {}

    void dontCatch()
    {
        catch_ = false;
    }

    virtual void Run()
    {
        if (catch_)
        {
            try
            {
                testing::Test::Run();
            }
            catch (fungi::IOException &e)
            {
                TearDown();
                FAIL() << "IOException: " << e.what();
            }
            catch (std::exception &e)
            {
                TearDown();
                FAIL() << "exception: " << e.what();
            }
            catch (...)
            {
                TearDown();
                FAIL() << "unknown exception";
            }
        }
        else
        {
            testing::Test::Run();
        }
    }

private:
    bool catch_;

    static unsigned xmlrpcport_;

public:
    static boost::filesystem::path
    getTempPath(const std::string& ipath);
};

}

#endif /* EXGTEST_H_ */

// Local Variables: **
// mode: c++ **
// End: **
