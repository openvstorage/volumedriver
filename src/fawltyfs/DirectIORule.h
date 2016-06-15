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

#ifndef DIRECT_IO_RULE_H_
#define DIRECT_IO_RULE_H_

#include "ErrorRules.h" // rule_id typedef

#include <memory>

namespace fawltyfs
{
struct DirectIORule
{
    DirectIORule(rule_id iid,
                 const std::string& ipath_regex,
                 bool idirect_io)
        : id(iid)
        , path_regex(ipath_regex)
        , direct_io(idirect_io)
    {}

    DirectIORule(const DirectIORule&) = delete;

    DirectIORule&
    operator=(const DirectIORule&) = delete;

    bool
    match(const std::string& path) const
    {
        return regex_match(path, path_regex);
    }

    const rule_id id;
    const boost::regex path_regex;
    const bool direct_io;
};

typedef std::shared_ptr<DirectIORule> DirectIORulePtr;
typedef std::list<DirectIORulePtr> direct_io_rules_list_t;

}

#endif // !DIRECT_IO_RULE_H_

// Local Variables: **
// compile-command: "cd .. && scons -j 5" **
// mode: c++ **
// End: **
