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

#include "FileEventRule.h"

#include <iostream>
#include <type_traits>

#include <youtils/StreamUtils.h>

namespace volumedriverfs
{

FileEventRule&
FileEventRule::operator=(const FileEventRule& other)
{
    if (this != &other)
    {
#define COPY(x)                                                      \
        const_cast<std::remove_const<decltype(x)>::type&>(x) = other.x

        COPY(call_set);
        COPY(path_regex_string);
        COPY(path_regex);

#undef COPY
    }

    return *this;
}

bool
FileEventRule::operator==(const FileEventRule& other) const
{
    return call_set == other.call_set and
        path_regex_string == other.path_regex_string and
        path_regex == other.path_regex;
}

bool
FileEventRule::operator!=(const FileEventRule& other) const
{
    return not operator==(other);
}

std::ostream&
operator<<(std::ostream& os,
           const FileEventRule& rule)
{
    os << "(" << rule.path_regex << ", ";
    youtils::StreamUtils::stream_out_sequence(os,
                                              rule.call_set);
    os << ")";

    return os;
}

std::ostream&
operator<<(std::ostream& os,
           const FileEventRules& rules)
{
    youtils::StreamUtils::stream_out_sequence(os,
                                              rules);
    return os;
}

}

namespace initialized_params
{

using Accessor = PropertyTreeVectorAccessor<volumedriverfs::FileEventRule>;
const std::string
Accessor::path_regex_key("fs_file_event_rule_path_regex");

const std::string
Accessor::call_array_key("fs_file_event_rule_calls");

const std::string
Accessor::call_key("");

}
