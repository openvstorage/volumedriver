// Copyright 2015 Open vStorage NV
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

#ifndef VFS_EVENT_PUBLISHER_H_
#define VFS_EVENT_PUBLISHER_H_

#include "AmqpTypes.h"
#include "ClusterId.h"
#include "FileSystemParameters.h"
#include "NodeId.h"

#include <mutex>

#include <boost/property_tree/ptree_fwd.hpp>

#include <SimpleAmqpClient/SimpleAmqpClient.h>

#include <youtils/InitializedParam.h>
#include <youtils/Logging.h>
#include <youtils/VolumeDriverComponent.h>

#include <volumedriver/Events.h>

namespace events
{

class Event;

}

namespace volumedriverfs
{

class EventPublisher
    : public youtils::VolumeDriverComponent
    , public events::PublisherInterface
{
public:
    explicit EventPublisher(const ClusterId& cluster_id,
                            const NodeId& node_id,
                            const boost::property_tree::ptree& pt,
                            const RegisterComponent registrate = RegisterComponent::T);

    virtual ~EventPublisher() = default;

    EventPublisher(const EventPublisher&) = delete;

    EventPublisher&
    operator=(const EventPublisher&) = delete;

    virtual void
    publish(const events::Event& ev) noexcept override final;

    bool
    enabled() const;

    virtual const char*
    componentName() const override final;

    virtual void
    update(const boost::property_tree::ptree& pt,
           youtils::UpdateReport& rep) override final;

    virtual void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault reportDefault) const override final;

    virtual bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final;

    // default taken from AmqpClient::Channel
    static constexpr unsigned max_frame_size = 131072;

private:
    DECLARE_LOGGER("EventPublisher");

    unsigned index_;
    AmqpClient::Channel::ptr_t channel_;

    DECLARE_PARAMETER(events_amqp_uris);
    DECLARE_PARAMETER(events_amqp_exchange);
    DECLARE_PARAMETER(events_amqp_routing_key);

    const ClusterId cluster_id_;
    const NodeId node_id_;

    // Protects the channel - the underlying -lrabbitmq-c is not thread safe.
    typedef std::mutex lock_type;
    mutable lock_type lock_;

    bool
    enabled_() const;
};

}

#endif // !VFS_EVENT_PUBLISHER_H_
