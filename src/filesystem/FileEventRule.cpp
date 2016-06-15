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
