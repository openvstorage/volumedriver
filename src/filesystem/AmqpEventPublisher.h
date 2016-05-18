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

#ifndef VFS_AMQP_EVENT_PUBLISHER_H_
#define VFS_AMQP_EVENT_PUBLISHER_H_

#include "AmqpTypes.h"
#include "ClusterId.h"
#include "FileSystemParameters.h"
#include "NodeId.h"

#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/thread/mutex.hpp>

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

class AmqpEventPublisher
    : public youtils::VolumeDriverComponent
    , public events::PublisherInterface
{
public:
    explicit AmqpEventPublisher(const ClusterId& cluster_id,
                                const NodeId& node_id,
                                const boost::property_tree::ptree& pt,
                                const RegisterComponent registrate = RegisterComponent::T);

    virtual ~AmqpEventPublisher() = default;

    AmqpEventPublisher(const AmqpEventPublisher&) = delete;

    AmqpEventPublisher&
    operator=(const AmqpEventPublisher&) = delete;

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
    DECLARE_LOGGER("AmqpEventPublisher");

    // Work around deficiencies in the AMQP lib(s) we're using:
    // SimpleAmqpClient's publish calls is blocking (NB: newer versions of
    // -lrabbitmq-c seem to have grown support for non-blocking sockets, cf.
    // https://github.com/alanxz/rabbitmq-c/pull/256) and there were several
    // occurences of calls being stuck waiting for a message from the server
    // side that never arrived. The TCP keepalive support added on top of it
    // does not solve the problem completely as in some cases the server was
    // in a zombie state, keeping the TCP connection open but never sending
    // the expected reply back.
    // That lead to interesting lockups with a simple locking scheme where a
    // management call was trying to update the configuration while another
    // thread was stuck in a call to SimpleAmqpClient while holding the lock.
    //
    // To safeguard us against this the following locking scheme is employed:
    // * mgmt_lock_ protects (besides the config params such as uris_) the
    //   Channel::ptr_t (boost::shared_ptr).
    // * channel_lock_ is used to prevent concurrent invocations of Channel
    //   methods from different threads as the underlying -lrabbitmq-c is not
    //   thread safe.
    //
    // In case the above doesn't make it clear already: the mgmt_lock_ should
    // not be held while the channel_ lock is acquired to call into
    // SimpleAmqpClient.
    //
    // That still leaves the problem that threads invoking publish could
    // lock up on the channel_lock_ - callers (cf. EventHandler) have to
    // deal with it.
    using Mutex = boost::mutex;
    mutable Mutex mgmt_lock_;
    mutable Mutex channel_lock_;

    // Indicates which entry of the events_amqp_uris vector is currently active.
    unsigned index_;
    AmqpClient::Channel::ptr_t channel_;

    boost::shared_ptr<initialized_params::PARAMETER_TYPE(events_amqp_uris)> uris_;
    boost::shared_ptr<initialized_params::PARAMETER_TYPE(events_amqp_exchange)> exchange_;
    boost::shared_ptr<initialized_params::PARAMETER_TYPE(events_amqp_routing_key)> routing_key_;

    const ClusterId cluster_id_;
    const NodeId node_id_;

    bool
    enabled_() const;

    void
    publish_(const events::Event&,
             const boost::shared_ptr<initialized_params::PARAMETER_TYPE(events_amqp_uris)>&,
             const boost::shared_ptr<initialized_params::PARAMETER_TYPE(events_amqp_exchange)>&,
             const boost::shared_ptr<initialized_params::PARAMETER_TYPE(events_amqp_routing_key)>&,
             AmqpClient::Channel::ptr_t&,
             unsigned& uris_index) const;
};

}

#endif // !VFS_AMQP_EVENT_PUBLISHER_H_
