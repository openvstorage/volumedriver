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

#ifndef SCO_CACHE_ACCESS_DATA_PERSISTOR_H_
#define SCO_CACHE_ACCESS_DATA_PERSISTOR_H_

#include "SCOCache.h"

namespace volumedriver
{

class SCOCacheAccessDataPersistor
{
public:
    explicit SCOCacheAccessDataPersistor(SCOCache& scoCache)
        : scocache_(scoCache)
    {}

    ~SCOCacheAccessDataPersistor() = default;

    SCOCacheAccessDataPersistor(const SCOCacheAccessDataPersistor&) = delete;

    SCOCacheAccessDataPersistor&
    operator=(const SCOCacheAccessDataPersistor&) = delete;

    void
    operator()();

private:
    DECLARE_LOGGER("SCOCacheAccessDataPersistor");

    SCOCache& scocache_;
};

}

#endif // !SCO_CACHE_ACCESS_DATA_PERSISTOR_H_

// Local Variables: **
// mode: c++ **
// End: **
