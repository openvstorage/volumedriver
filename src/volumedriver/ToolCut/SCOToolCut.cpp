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

#include "SCOToolCut.h"

namespace toolcut
{
SCOToolCut::SCOToolCut(const volumedriver::SCO& sco)
    :sco_(sco)
{}

SCOToolCut::SCOToolCut(const std::string& i_str)
    :sco_(i_str)
{}

std::string
SCOToolCut::str() const
{
    return sco_.str();
}

uint8_t
SCOToolCut::version() const
{
    return sco_.version();
}


uint8_t
SCOToolCut::cloneID() const
{
    return sco_.cloneID();
}


volumedriver::SCONumber
SCOToolCut::number() const
{
    return sco_.number();
}


bool
SCOToolCut::isSCOString(const std::string& str)
{
    return volumedriver::SCO::isSCOString(str);
}


bool
SCOToolCut::asBool() const
{
    return sco_.asBool();
}
}

// Local Variables: **
// mode: c++ **
// End: **
