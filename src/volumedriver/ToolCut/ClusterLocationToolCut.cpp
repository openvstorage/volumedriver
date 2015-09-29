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

#include "ClusterLocationToolCut.h"
#include "SCOToolCut.h"
#include <iostream>
namespace toolcut
{

ClusterLocationToolCut::ClusterLocationToolCut(const volumedriver::ClusterLocation& loc)
    : loc_(loc)
{}


ClusterLocationToolCut::ClusterLocationToolCut(const std::string& name)
    : loc_(name)
{}

bool
ClusterLocationToolCut::isClusterLocationString(const std::string& str)
{
    return volumedriver::ClusterLocation::isClusterLocationString(str);
}

uint16_t
ClusterLocationToolCut::offset()
{
    return loc_.offset();
}

uint8_t
ClusterLocationToolCut::version()
{
    return loc_.version();
}

uint8_t
ClusterLocationToolCut::cloneID()
{
    return loc_.cloneID();
}

volumedriver::SCONumber
ClusterLocationToolCut::number()
{
    return loc_.number();
}

std::string
ClusterLocationToolCut::str() const
{
    return loc_.str();
}

SCOToolCut*
ClusterLocationToolCut::sco()
{
    return new SCOToolCut(loc_.sco());
}

std::string
ClusterLocationToolCut::repr() const
{
    return str();
}


}

// Local Variables: **
// mode: c++ **
// End: **
