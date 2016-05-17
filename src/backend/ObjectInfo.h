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

#ifndef OBJECT_INFO_H_
#define OBJECT_INFO_H_

#include <string>
#include <map>
#include <youtils/CheckSum.h>
#include <youtils/StrongTypedString.h>

STRONG_TYPED_STRING(backend, ETag);

namespace backend
{
struct ObjectInfo
{

    ObjectInfo(const std::string& status,
               youtils::CheckSum checksum,
               uint64_t size,
               const std::string& etag,
               const std::map<std::string, std::string>& meta = std::map<std::string, std::string>())
        : status_(status)
        , checksum_(checksum)
        , size_(size)
        , etag_(etag)
        , customMeta_(meta)
    {}

    const std::string status_;
    const youtils::CheckSum checksum_;
    const uint64_t size_;
    const backend::ETag etag_;

    typedef std::map<std::string, std::string> CustomMetaData;
    const CustomMetaData customMeta_;
};
}

#endif

// Local Variables: **
// mode: c++ **
// End: **
