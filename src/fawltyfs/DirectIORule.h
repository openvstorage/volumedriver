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
