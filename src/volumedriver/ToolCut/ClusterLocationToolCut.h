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

#ifndef CLUSTER_LOCATION_TOOLCUT_H_
#define CLUSTER_LOCATION_TOOLCUT_H_

#include "../ClusterLocation.h"
namespace toolcut
{

class SCOToolCut;

class ClusterLocationToolCut
{
public:
    ClusterLocationToolCut(const volumedriver::ClusterLocation& loc);

    ClusterLocationToolCut(const std::string& name);

    static bool
    isClusterLocationString(const std::string& str);

    uint16_t
    offset();

    uint8_t
    version();

    uint8_t
    cloneID();

    volumedriver::SCONumber
    number();

    std::string
    str() const;

    SCOToolCut*
    sco();

    std::string
    repr() const;


private:
    volumedriver::ClusterLocation loc_;
};
}


#endif

// Local Variables: **
// mode: c++ **
// End: **
