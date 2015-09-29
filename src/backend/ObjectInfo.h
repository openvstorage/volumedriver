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
