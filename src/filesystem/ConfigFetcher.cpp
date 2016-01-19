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

#include "ConfigFetcher.h"

#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <etcdcpp/etcd.hpp>

#include <youtils/EtcdReply.h>
#include <youtils/EtcdUrl.h>
#include <youtils/VolumeDriverComponent.h>

namespace volumedriverfs
{

namespace bpt = boost::property_tree;
namespace yt = youtils;

std::string
ConfigFetcher::parse_config(const yt::EtcdReply::Records& recs)
{
    bpt::ptree pt;
    std::stringstream ss;

    for (const auto& rec: recs)
    {
        if (not rec.second.dir)
        {
            bpt::ptree child;
            if (rec.second.value)
            {
                std::stringstream st;
                st << *rec.second.value;
                bpt::read_json(st, child);
            }
            std::string tmp_key = rec.second.key;
            pt.add_child(tmp_key.substr(tmp_key.find_last_of("/") + 1),
                         child);
        }
    }
    bpt::write_json(ss, pt);
    return ss.str();
}

bpt::ptree
ConfigFetcher::operator()(VerifyConfig verify_config)
{
    LOG_INFO("Fetching config from " << config_);

    boost::optional<yt::EtcdUrl> etcd_url;

    try
    {
        etcd_url = boost::lexical_cast<yt::EtcdUrl>(config_);
    }
    catch (...)
    {}

    if (etcd_url)
    {
        etcd::Client<yt::EtcdReply> client(etcd_url->host,
                                           etcd_url->port);

        const yt::EtcdReply reply(client.Get(etcd_url->key));
        const boost::optional<yt::EtcdReply::Error> err(reply.error());
        if (err)
        {
            LOG_ERROR("Etcd reported error retrieving " << *etcd_url <<
                      ": code " << err->error_code <<
                      ", message " << err->message <<
                      ", cause " << err->cause);
            throw Exception("Failed to retrieve config from etcd");
        }

        const boost::optional<yt::EtcdReply::Node> node(reply.node());
        if (not node)
        {
            LOG_ERROR("Failed to find desired node in " << *etcd_url);
            throw Exception("Failed to find node in etcd");
        }

        const yt::EtcdReply::Records recs(reply.records());
        if (recs.size() < 2)
        {
            LOG_ERROR("Failed to find config subsections under " << *etcd_url);
            throw Exception("Failed to find config subsections");
        }
        std::stringstream ss(parse_config(recs));
        return yt::VolumeDriverComponent::read_config(ss,
                                                      verify_config);
    }
    else
    {
        return yt::VolumeDriverComponent::read_config_file(config_,
                                                           verify_config);
    }
}

}
