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
