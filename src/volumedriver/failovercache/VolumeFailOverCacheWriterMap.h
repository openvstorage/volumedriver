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

#ifndef VOLUMEFAILOVERCACHEWRITERMAP_H
#define VOLUMEFAILOVERCACHEWRITERMAP_H

#include "FailOverCacheWriter.h"

#include "../Types.h"

#include "fungilib/Mutex.h"

#include <map>
#include <string>

#include <youtils/FileUtils.h>
#include <youtils/Logging.h>

namespace failovercache
{

class VolumeFailOverCacheWriterMap
    : private std::map<std::string,FailOverCacheWriter*>
{
public:
    explicit VolumeFailOverCacheWriterMap(const boost::filesystem::path& root);

    ~VolumeFailOverCacheWriterMap();

    FailOverCacheWriter*
    lookup(const std::string& namespaceName,
           const volumedriver::ClusterSize& cluster_size);

    void
    remove(const FailOverCacheWriter*);

private:
    DECLARE_LOGGER("FailOverCacheWriter");

    const boost::filesystem::path root_;
};

}

// Local Variables: **
// mode: c++ **
// End: **


#endif // VOLUMEFAILOVERCACHEWRITERMAP_H
