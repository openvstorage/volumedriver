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

#ifndef VFS_FILE_EVENT_RULE_H_
#define VFS_FILE_EVENT_RULE_H_

#include "FileSystemCall.h"
#include "FrontendPath.h"

#include <set>

#include <boost/regex.hpp>

#include <youtils/InitializedParam.h>
#include <youtils/Logging.h>

namespace volumedriverfs
{

class FileEventRule
{
public:
    FileEventRule(const std::set<FileSystemCall>& calls,
                  const std::string& path_regex_str)
        : call_set(calls)
        , path_regex_string(path_regex_str)
        , path_regex(path_regex_string)
    {}

    ~FileEventRule() = default;

    FileEventRule(const FileEventRule& other) = default;

    FileEventRule&
    operator=(const FileEventRule& other);

    bool
    operator==(const FileEventRule& other) const;

    bool
    operator!=(const FileEventRule& other) const;

    bool
    match(const FileSystemCall call,
          const FrontendPath& path) const
    {
        return (call_set.find(call) != call_set.end()) and
            boost::regex_match(path.str(), path_regex);
    }

    const std::set<FileSystemCall> call_set;
    const std::string path_regex_string;
    const boost::regex path_regex;

private:
    DECLARE_LOGGER("FileEventRule");
};

using FileEventRules = std::vector<FileEventRule>;

std::ostream&
operator<<(std::ostream& os,
           const FileEventRule& rule);

std::ostream&
operator<<(std::ostream& os,
           const FileEventRules& rule);

}

namespace initialized_params
{

template<>
struct PropertyTreeVectorAccessor<volumedriverfs::FileEventRule>
{
    DECLARE_LOGGER("FileEventRulePropertyTreeVectorAccessor");

    static const std::string path_regex_key;
    static const std::string call_array_key;
    static const std::string call_key;

    using Type = volumedriverfs::FileEventRule;

    static Type
    get(const boost::property_tree::ptree& pt)
    {
        std::set<volumedriverfs::FileSystemCall> calls;

        for (const auto& child : pt.get_child(call_array_key))
        {
            auto maybe_call(child.second.get<volumedriverfs::MaybeFileSystemCall>(call_key));
            VERIFY(maybe_call); // otherwise the pt.get() should've thrown.

            const auto r(calls.insert(*maybe_call));
            if (r.second == false)
            {
                throw fungi::IOException("Duplicate entry in FileSystemCall set");
            }
        }

        return Type(calls,
                    pt.get<std::string>(path_regex_key));
    }

    static void
    put(boost::property_tree::ptree& pt,
        const Type& rule)
    {
        boost::property_tree::ptree children;

        for (const auto& call : rule.call_set)
        {
            boost::property_tree::ptree child;
            child.put("", call);
            children.push_back(std::make_pair(std::string(call_key), child));
        }

        pt.add_child(call_array_key, children);
        pt.put(path_regex_key, rule.path_regex_string);
    }
};

}

#endif // !VFS_FILE_EVENT_RULE_H_
