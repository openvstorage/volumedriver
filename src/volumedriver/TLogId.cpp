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

#include "TLogId.h"

#include <iostream>

namespace volumedriver
{

namespace yt = youtils;

namespace
{

const std::string tlog_prefix("tlog_");

}

std::ostream&
operator<<(std::ostream& os,
           const TLogId& tlog_id)
{
    return os << tlog_prefix << tlog_id.t;
}

std::istream&
operator>>(std::istream& is,
           TLogId& tlog_id)
{
    std::string s;
    is >> s;
    if (s.substr(0, tlog_prefix.size()) == tlog_prefix)
    {
        try
        {
            yt::UUID uuid(s.substr(tlog_prefix.size()));
            tlog_id = std::move(uuid);
        }
        catch (...)
        {
            is.setstate(std::ios_base::failbit);
        }
    }
    else
    {
        is.setstate(std::ios_base::failbit);
    }

    return is;
}

}
