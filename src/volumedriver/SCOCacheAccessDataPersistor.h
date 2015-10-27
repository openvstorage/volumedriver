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
