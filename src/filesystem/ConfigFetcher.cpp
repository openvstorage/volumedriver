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

#include <etcdcpp/etcd.hpp>

#include <youtils/EtcdReply.h>
#include <youtils/EtcdUrl.h>
#include <youtils/VolumeDriverComponent.h>

namespace volumedriverfs
{

namespace bpt = boost::property_tree;
namespace yt = youtils;

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

        if (not node->value)
        {
            LOG_ERROR("Failed to find desired value of " << *etcd_url);
            throw Exception("Failed to find value of etcd key");
        }

        std::stringstream ss(*node->value);
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
