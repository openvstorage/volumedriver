// Copyright 2016 iNuron NV
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

#include "EtcdReply.h"

#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <etcdcpp/etcd.hpp>

namespace youtils
{

namespace bpt = boost::property_tree;

using namespace std::literals::string_literals;

namespace
{

bpt::ptree
read_ptree(const std::string& json)
{
    std::stringstream ss(json);
    bpt::ptree pt;

    bpt::json_parser::read_json(ss,
                                pt);

    return pt;
}

const std::string cause_key("cause");
const std::string created_index_key("createdIndex");
const std::string dir_key("dir");
const std::string error_code_key("errorCode");
const std::string index_key("index");
const std::string key_key("key");
const std::string message_key("message");
const std::string modified_index_key("modifiedIndex");
const std::string prev_node_key("prevNode");
const std::string node_key("node");
const std::string nodes_key("nodes");
const std::string ttl_key("ttl");
const std::string value_key("value");

}

EtcdReply::EtcdReply(const std::string& json)
    : ptree_(read_ptree(json))
{}

EtcdReply::EtcdReply(const std::string& /* header */,
                     const std::string& json)
    : EtcdReply(json)
{}

boost::optional<EtcdReply::Error>
EtcdReply::error() const
{
    const auto ec(ptree_.get_optional<uint64_t>(error_code_key));
    if (ec)
    {
        return Error(*ec,
                     ptree_.get<std::string>(message_key,
                                             "(message missing!)"s),
                     ptree_.get<std::string>(cause_key,
                                             "(cause missing!)"s),
                     ptree_.get_optional<uint64_t>(index_key));
    }
    else
    {
        return boost::none;
    }
}

namespace
{

EtcdReply::Node
extract_node(const bpt::ptree& pt)
{
    return EtcdReply::Node(pt.get<std::string>(key_key),
                           pt.get_optional<std::string>(value_key),
                           pt.get<bool>(dir_key,
                                        false),
                           pt.get_optional<uint64_t>(created_index_key),
                           pt.get_optional<uint64_t>(modified_index_key),
                           pt.get_optional<uint64_t>(ttl_key));
}

boost::optional<EtcdReply::Node>
maybe_extract_node(const bpt::ptree& pt,
                   const std::string& key)
{
    const auto child(pt.get_child_optional(key));
    if (child)
    {
        return extract_node(*child);
    }
    else
    {
        return boost::none;
    }
}

void
extract_records(const bpt::ptree& pt,
                EtcdReply::Records& recs)
{
    const auto k(pt.get_optional<std::string>(key_key));

    if (k)
    {
        recs.emplace(std::make_pair(*k,
                                    extract_node(pt)));
    }

    const auto child(pt.get_child_optional(node_key));
    if (child)
    {
        extract_records(*child,
                        recs);
    }
    else
    {
        const auto children(pt.get_child_optional(nodes_key));
        if (children)
        {
            for (const auto& child : *children)
            {
                extract_records(child.second,
                                recs);
            }
        }
    }
}

}

EtcdReply::Records
EtcdReply::records() const
{
    Records recs;
    extract_records(ptree_,
                    recs);
    return recs;
}

boost::optional<EtcdReply::Node>
EtcdReply::node() const
{
    return maybe_extract_node(ptree_,
                              node_key);
}

boost::optional<EtcdReply::Node>
EtcdReply::prev_node() const
{
    return maybe_extract_node(ptree_,
                              prev_node_key);
}

}
