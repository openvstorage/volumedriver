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
