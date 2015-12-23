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

#include "FileSystemEventTestSetup.h"

#include <boost/property_tree/ptree.hpp>

#include <youtils/InitializedParam.h>
#include <youtils/UUID.h>

#include "../FileSystemParameters.h"

namespace volumedriverfstest
{

namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace vfs = volumedriverfs;
namespace yt = youtils;

vfs::AmqpUri FileSystemEventTestSetup::amqp_uri_;

FileSystemEventTestSetup::FileSystemEventTestSetup(const std::string& name)
    : name_(name)
    , amqp_exchange_(name + std::string("-exchange-") + yt::UUID().str())
    , amqp_routing_key_(name + std::string("-queue-") + yt::UUID().str())
    , cluster_id_(name + "-cluster")
    , node_id_(name + "-node")
{
    if (use_amqp())
    {
        channel_ = AmqpClient::Channel::CreateFromUri(amqp_uri(),
                                                      vfs::AmqpEventPublisher::max_frame_size);
        channel_->DeclareExchange(amqp_exchange(),
                                  AmqpClient::Channel::EXCHANGE_TYPE_FANOUT);

        channel_->DeclareQueue(amqp_routing_key_);
        channel_->BindQueue(amqp_routing_key_, amqp_exchange());
        channel_->BasicConsume(amqp_routing_key_, amqp_routing_key_);
    }

    bpt::ptree pt;
    publisher_ = std::make_unique<vfs::AmqpEventPublisher>(cluster_id_,
                                                           node_id_,
                                                           make_config(pt));
}

FileSystemEventTestSetup::~FileSystemEventTestSetup()
{
    if (channel_)
    {
        try
        {
            channel_->DeleteQueue(amqp_routing_key_);
            channel_->DeleteExchange(amqp_exchange());
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to tear down AMQP queue and exchange (manual cleanup necessary?)");
    }
}

bpt::ptree&
FileSystemEventTestSetup::make_config(bpt::ptree& pt,
                                      const std::vector<vfs::AmqpUri>& uris,
                                      const vfs::AmqpExchange& exchange,
                                      const vfs::AmqpRoutingKey& routing_key)
{
    ip::PARAMETER_TYPE(events_amqp_uris)(uris).persist(pt);
    ip::PARAMETER_TYPE(events_amqp_exchange)(exchange).persist(pt);
    ip::PARAMETER_TYPE(events_amqp_routing_key)(routing_key).persist(pt);

    return pt;
}

bpt::ptree&
FileSystemEventTestSetup::make_config(bpt::ptree& pt) const
{
    std::vector<vfs::AmqpUri> uris;
    if (not amqp_uri_.empty())
    {
        uris.emplace_back(amqp_uri_);
    }

    return make_config(pt,
                       uris,
                       amqp_exchange_,
                       amqp_routing_key_);
}

}
