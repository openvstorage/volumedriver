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

#ifndef VD_TEST_CONFIG_H_
#define VD_TEST_CONFIG_H_

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

}

#endif /* VD_TEST_CONFIG_H_ */

// Local Variables: **
// mode: c++ **
// End: **
