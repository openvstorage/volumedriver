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

#ifndef DEBUG_PRINT_H_
#define DEBUG_PRINT_H_
#ifndef NDEBUG
#include <boost/property_tree/ptree.hpp>
namespace volumedriver
{
class NSIDMap;
class ClusterLocation;
class CacheEntry;
class FailOverWriter;
class VolumeConfig;
}

namespace debug
{

void
DumpPropertyTree(const boost::property_tree::ptree& pt);

void
DPVolumeConfig(const volumedriver::VolumeConfig* v);

// void
// DPNSIDMap(const volumedriver::NSIDMap* m);

void
DPClusterLocation(const volumedriver::ClusterLocation* c);

void
DPCacheEntry(const volumedriver::CacheEntry* e);

void
DPFailOverCache(const volumedriver::FailOverWriter*);

void
DumpStringStream(std::stringstream&);

}

#define DEBUG_FUNCTIONS                                          \
    namespace debug                                              \
    {                                                            \
        void *p1 = reinterpret_cast<void*>(DPVolumeConfig);      \
        void *p3 = reinterpret_cast<void*>(DPClusterLocation);   \
        void *p4 = reinterpret_cast<void*>(DumpPropertyTree);    \
    }
#else
#define DEBUG_FUNCTIONS

#endif // NDEBUG
#endif // DEBUG_PRINT_H_
