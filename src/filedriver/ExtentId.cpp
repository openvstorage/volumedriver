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
