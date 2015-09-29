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

#include "ExtentId.h"

namespace filedriver
{

const std::string
ExtentId::separator_(".");

const size_t
ExtentId::suffix_size_ = 8;

ExtentId::ExtentId(const std::string& str)
    : container_id("<uninitialized>")
    , offset(std::numeric_limits<decltype(offset)>::max())
{
    if (str.size() < separator_.size() + suffix_size_)
    {
        throw NotAnExtentId("Not an ExtentId: too short",
                            str.c_str());
    }

    const size_t cid_size = str.size() - suffix_size_ - separator_.size();
    const_cast<ContainerId&>(container_id) = ContainerId(str.substr(0, cid_size));

    const std::string sep(str.substr(cid_size, separator_.size()));
    if (sep != separator_)
    {
        throw NotAnExtentId("Not an ExtentId: wrong separator",
                            str.c_str());
    }

    const std::string suffix(str.substr(cid_size + sep.size()));

    std::stringstream ss;
    ss << suffix;

    uint32_t off;
    ss >> std::hex >> std::setfill('0') >> std::setw(suffix_size_) >> off;

    try
    {
        const_cast<uint32_t&>(offset) = off;
    }
    catch (...)
    {
        throw NotAnExtentId("Not an ExtentId: failed to parse suffix");
    }
}

}
