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

#ifndef YT_ETCD_REPLY_H_
#define YT_ETCD_REPLY_H_

#include "Logging.h"

#include <unordered_map>

#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>

namespace youtils
{

class EtcdReply
{
public:
    EtcdReply(const std::string& header,
              const std::string& json);

    explicit EtcdReply(const std::string& json);

    ~EtcdReply() = default;

    EtcdReply(const EtcdReply&) = default;

    EtcdReply&
    operator=(const EtcdReply&) = default;

    EtcdReply(EtcdReply&&) = default;

    EtcdReply&
    operator=(EtcdReply&&) = default;

    const boost::property_tree::ptree&
    ptree() const
    {
        return ptree_;
    }

    struct Error
    {
        uint64_t error_code;
        std::string message;
        std::string cause;
        boost::optional<uint64_t> index;

        Error(unsigned ec,
              const std::string& m,
              const std::string& c,
              const boost::optional<uint64_t>& idx)
            : error_code(ec)
            , message(m)
            , cause(c)
            , index(idx)
        {}
    };

    boost::optional<Error>
    error() const;

    struct Node
    {
        std::string key;
        boost::optional<std::string> value;
        bool dir;
        boost::optional<uint64_t> created_index;
        boost::optional<uint64_t> modified_index;
        boost::optional<uint64_t> ttl;

        Node(const std::string& k,
             const boost::optional<std::string>& v = boost::none,
             bool d = false,
             const boost::optional<uint64_t>& c = boost::none,
             const boost::optional<uint64_t>& m = boost::none,
             const boost::optional<uint64_t>& t = boost::none)
            : key(k)
            , value(v)
            , dir(d)
            , created_index(c)
            , modified_index(m)
            , ttl(t)
        {}

        ~Node() = default;

        Node(const Node&) = default;

        Node(Node&&) = default;

        Node&
        operator=(const Node&) = default;

        Node&
        operator=(Node&&) = default;

        bool
        operator==(const Node& other) const
        {
            return
                created_index == other.created_index and
                modified_index == other.modified_index and
                key == other.key and
                value == other.value and
                dir == other.dir and
                ttl == other.ttl;
        }

        bool
        operator!=(const Node& other) const
        {
            return not operator==(other);
        }
    };

    boost::optional<Node>
    node() const;

    boost::optional<Node>
    prev_node() const;

    using Records = std::unordered_map<std::string, Node>;

    Records
    records() const;

private:
    DECLARE_LOGGER("EtcdReply");

    boost::property_tree::ptree ptree_;
};

}

#endif // !YT_ETCD_REPLY_H_
