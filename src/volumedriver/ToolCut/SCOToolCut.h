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

#ifndef SCO_TOOL_CUT_H
#define SCO_TOOL_CUT_H
#include "../SCO.h"

namespace toolcut
{
class SCOToolCut
{
public:
    SCOToolCut(const volumedriver::SCO& sco);

    SCOToolCut(const std::string& i_str);

    std::string
    str() const;

    uint8_t
    version() const;


    uint8_t
    cloneID() const;

    volumedriver::SCONumber
    number() const;

    static bool
    isSCOString(const std::string& str);

    bool
    asBool() const;

private:
    volumedriver::SCO sco_;

};

}

#endif // SCO_TOOL_CUT_H

// Local Variables: **
// mode: c++ **
// End: **
