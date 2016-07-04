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

#ifndef VFS_EVENT_TEST_SETUP_H_
#define VFS_EVENT_TEST_SETUP_H_

#include <boost/property_tree/ptree_fwd.hpp>

#include <SimpleAmqpClient/SimpleAmqpClient.h>

#include <youtils/Logging.h>
#include <gtest/gtest.h>

#include "../AmqpTypes.h"
#include "../ClusterId.h"
#include "../AmqpEventPublisher.h"
#include "../FileSystemEvents.h"
#include "../NodeId.h"

namespace volumedriverfstest
{

class FileSystemEventTestSetup
{
public:
    explicit FileSystemEventTestSetup(const std::string& name);

    ~FileSystemEventTestSetup();

    FileSystemEventTestSetup(const FileSystemEventTestSetup&) = delete;

    FileSystemEventTestSetup&
    operator=(FileSystemEventTestSetup&) = delete;

    static void
    set_amqp_uri(const volumedriverfs::AmqpUri& uri)
    {
        amqp_uri_ = uri;
    }

    static volumedriverfs::AmqpUri
    amqp_uri()
    {
        return amqp_uri_;
    }

    static bool
    use_amqp()
    {
        return not amqp_uri_.empty();
    }

    volumedriverfs::AmqpExchange
    amqp_exchange() const
    {
        return amqp_exchange_;
    }

    volumedriverfs::AmqpRoutingKey
    amqp_routing_key() const
    {
        return amqp_routing_key_;
    }

    static boost::property_tree::ptree&
    make_config(boost::property_tree::ptree& pt,
                const std::vector<volumedriverfs::AmqpUri>& uris,
                const volumedriverfs::AmqpExchange& exchange,
                const volumedriverfs::AmqpRoutingKey& routing_key);

    boost::property_tree::ptree&
    make_config(boost::property_tree::ptree& pt) const;

    void
    publish_event(const events::Event& ev)
    {
        publisher_->publish(ev);
    }

    volumedriverfs::ClusterId
    cluster_id() const
    {
        return cluster_id_;
    }

    volumedriverfs::NodeId
    node_id() const
    {
        return node_id_;
    }

    events::EventMessage
    get_message()
    {
        AmqpClient::Envelope::ptr_t envelope(channel_->BasicConsumeMessage());
        events::EventMessage msg;

        EXPECT_TRUE(msg.ParseFromString(envelope->Message()->Body()));
        EXPECT_TRUE(msg.has_cluster_id());
        EXPECT_TRUE(msg.has_node_id());

        return msg;
    }

    // XXX: this relies on typedefs in a google::protobuf::internal namespace, which
    // suggests that this is not exactly a stable API.
    template<typename T>
    typename std::remove_pointer<typename T::TypeTraits::MutableType>::type
    get_event(const T& t)
    {
        const events::EventMessage msg(get_message());
        return get_event(t,
                         msg);
    }

    template<typename T>
    typename std::remove_pointer<typename T::TypeTraits::MutableType>::type
    get_event(const T& t,
              const events::EventMessage& msg)
    {
        EXPECT_TRUE(msg.has_event());

        const auto& ev(msg.event());
        EXPECT_TRUE(ev.HasExtension(t));

        const auto ext(ev.GetExtension(t));
        return ext;
    }

protected:
    AmqpClient::Channel::ptr_t channel_;
    std::unique_ptr<volumedriverfs::AmqpEventPublisher> publisher_;

private:
    DECLARE_LOGGER("FileSystemEventTestSetup");

    static volumedriverfs::AmqpUri amqp_uri_;

    const std::string name_;
    const volumedriverfs::AmqpExchange amqp_exchange_;
    const volumedriverfs::AmqpRoutingKey amqp_routing_key_;

    const volumedriverfs::ClusterId cluster_id_;
    const volumedriverfs::NodeId node_id_;
};

}

#endif // VFS_EVENT_TEST_SETUP_H_
