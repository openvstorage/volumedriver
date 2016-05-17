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

#ifndef VD_CLUSTER_CACHE_BEHAVIOUR_H_
#define VD_CLUSTER_CACHE_BEHAVIOUR_H_

#include <iosfwd>

namespace volumedriver
{

enum class ClusterCacheBehaviour
{
    // We are using 0 in serialization to
    CacheOnWrite = 1,
    CacheOnRead = 2,
    NoCache = 3
};

std::ostream&
operator<<(std::ostream&,
           const ClusterCacheBehaviour a);

std::istream&
operator>>(std::istream&,
           ClusterCacheBehaviour&);

}

#endif // !VD_CLUSTER_CACHE_BEHAVIOUR_H_
