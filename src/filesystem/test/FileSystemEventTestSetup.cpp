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
