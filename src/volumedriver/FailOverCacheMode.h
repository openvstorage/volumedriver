// Copyright 2015 Open vStorage NV
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

#ifndef VD_FAILOVER_CACHE_MODE_H_
#define VD_FAILOVER_CACHE_MODE_H_

#include <iosfwd>
#include <cstdint>

namespace volumedriver
{

enum class FailOverCacheMode: uint8_t
{
    // We are using 0 in serialization to
    Asynchronous = 1,
    Synchronous = 2,
};

std::ostream&
operator<<(std::ostream&,
           const FailOverCacheMode a);

std::istream&
operator>>(std::istream&,
        FailOverCacheMode&);

}

#endif // !VD_FAILOVER_CACHE_MODE_H_
