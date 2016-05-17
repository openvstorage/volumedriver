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

#include "AmqpEventPublisher.h"
#include "FileSystemEvents.pb.h"

#include <netinet/tcp.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/thread/lock_guard.hpp>

#include <amqp.h>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/ScopeExit.h>
#include <youtils/System.h>

namespace volumedriverfs
{

#define LOCK_MGMT()                                             \
    boost::lock_guard<decltype(mgmt_lock_)> gml__(mgmt_lock_)

#define LOCK_CHANNEL()                                                  \
    boost::lock_guard<decltype(channel_lock_)> gcl__(channel_lock_)

namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace yt = youtils;

namespace
{

DECLARE_LOGGER("AmqpEventPublisherUtils");

void
check_uri(const AmqpUri& uri)
{
    // not exposed by SimpleAmqpClient so we need to get our hands dirty with the gritty
    // details of rabbitmq-c
    std::vector<char> buf(uri.size() + 1, 0);

    memcpy(buf.data(),
           uri.data(),
           uri.size());

    amqp_connection_info info;
    memset(&info, 0x0, sizeof(info));

    const int ret = ::amqp_parse_url(buf.data(),
                                     &info);

    if (ret == AMQP_STATUS_BAD_URL)
    {
        LOG_ERROR(uri << " is not a valid AMQP uri");
        throw fungi::IOException("Invalid AMQP URI",
                                 uri.str().c_str());
    }
}

void
check_uris(const AmqpUris& uris)
{
    std::for_each(uris.begin(),
                  uris.end(),
                  check_uri);
}

void
enable_keepalive(AmqpClient::Channel& channel)
{
    int sock = channel.SockFd();

    static const int keep_cnt =
        yt::System::get_env_with_default("AMQP_CLIENT_KEEPCNT",
                                         5);

    static const int keep_idle =
        yt::System::get_env_with_default("AMQP_CLIENT_KEEPIDLE",
                                         60);

    static const int keep_intvl =
        yt::System::get_env_with_default("AMQP_CLIENT_KEEPINTVL",
                                         60);

    yt::System::setup_tcp_keepalive(sock,
                                    keep_cnt,
                                    keep_idle,
                                    keep_intvl);
}

template<typename T>
boost::shared_ptr<typename T::ValueType>
extract_param(const bpt::ptree& pt)
{
    return boost::make_shared<typename T::ValueType>(T(pt).value());
}

}

AmqpEventPublisher::AmqpEventPublisher(const ClusterId& cluster_id,
                                       const NodeId& node_id,
                                       const bpt::ptree& pt,
                                       const RegisterComponent registrate)
    : VolumeDriverComponent(registrate, pt)
    , index_(0)
    , uris_(extract_param<ip::PARAMETER_TYPE(events_amqp_uris)>(pt))
    , exchange_(extract_param<ip::PARAMETER_TYPE(events_amqp_exchange)>(pt))
    , routing_key_(extract_param<ip::PARAMETER_TYPE(events_amqp_routing_key)>(pt))
    , cluster_id_(cluster_id)
    , node_id_(node_id)
{
    check_uris(*uris_);

    if (not enabled_())
    {
        LOG_WARN("No AMQP URI specified - event notifications will be discarded");
    }
}

bool
AmqpEventPublisher::enabled() const
{
    LOCK_MGMT();
    return enabled_();
}

bool
AmqpEventPublisher::enabled_() const
{
    VERIFY(uris_ != nullptr);
    return not uris_->empty();
}

const char*
AmqpEventPublisher::componentName() const
{
    return initialized_params::events_component_name;
}

namespace
{

template<typename T>
void
do_update(boost::shared_ptr<typename T::ValueType>& ptr,
          const bpt::ptree& pt,
          yt::UpdateReport& urep,
          bool& updated)
{
    VERIFY(ptr != nullptr);

    T t(*ptr);
    const T u(pt);

    if (u.value() != t.value())
    {
        updated = true;
        ptr = boost::make_shared<typename T::ValueType>(u.value());
    }

    t.update(pt,
             urep);
}

}

void
AmqpEventPublisher::update(const bpt::ptree& pt,
                           yt::UpdateReport& urep)
{
    LOCK_MGMT();

    bool updated = false;

    do_update<ip::PARAMETER_TYPE(events_amqp_uris)>(uris_,
                                                    pt,
                                                    urep,
                                                    updated);

    do_update<ip::PARAMETER_TYPE(events_amqp_exchange)>(exchange_,
                                                        pt,
                                                        urep,
                                                        updated);

    do_update<ip::PARAMETER_TYPE(events_amqp_routing_key)>(routing_key_,
                                                           pt,
                                                           urep,
                                                           updated);

    if (updated)
    {
        index_ = 0;
        channel_ = nullptr;
    }
}

namespace
{

template<typename T>
void
do_persist(const boost::shared_ptr<typename T::ValueType>& ptr,
           bpt::ptree& pt,
           const ReportDefault report_default)
{
    VERIFY(ptr != nullptr);
    T t(*ptr);
    t.persist(pt,
              report_default);
}

}

void
AmqpEventPublisher::persist(bpt::ptree& pt,
                            const ReportDefault report_default) const
{
    LOCK_MGMT();

    do_persist<ip::PARAMETER_TYPE(events_amqp_uris)>(uris_,
                                                     pt,
                                                     report_default);
    do_persist<ip::PARAMETER_TYPE(events_amqp_exchange)>(exchange_,
                                                         pt,
                                                         report_default);

    do_persist<ip::PARAMETER_TYPE(events_amqp_routing_key)>(routing_key_,
                                                            pt,
                                                            report_default);
}

bool
AmqpEventPublisher::checkConfig(const bpt::ptree& pt,
                                yt::ConfigurationReport& crep) const
{
    const ip::PARAMETER_TYPE(events_amqp_uris) uris(pt);
    try
    {
        check_uris(uris.value());
        return true;
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("URI check failed: " << EWHAT);
            crep.push_back(yt::ConfigurationProblem(uris.name(),
                                                    uris.section_name(),
                                                    EWHAT));
            return false;
        });
}

void
AmqpEventPublisher::publish(const events::Event& ev) noexcept
{
    try
    {
        ASSERT(ev.IsInitialized());

        boost::unique_lock<decltype(mgmt_lock_)> u(mgmt_lock_);

        if (enabled_())
        {
            // create copies so any config changes during the unlocked
            // section below do not interfere.
            boost::shared_ptr<AmqpUris> uris(uris_);
            boost::shared_ptr<AmqpExchange> exchange(exchange_);
            boost::shared_ptr<AmqpRoutingKey> routing_key(routing_key_);
            unsigned idx = index_;

            VERIFY(uris != nullptr);
            VERIFY(exchange != nullptr);
            VERIFY(routing_key != nullptr);
            VERIFY(idx < uris->size());

            AmqpClient::Channel::ptr_t chan = channel_;

            {
                u.unlock();
                auto on_exit(yt::make_scope_exit([&]
                                                 {
                                                     u.lock();
                                                 }));
                publish_(ev,
                         uris,
                         exchange,
                         routing_key,
                         chan,
                         idx);
            }

            if (uris == uris_ and
                exchange == exchange_ and
                routing_key == routing_key_)
            {
                VERIFY(idx < uris_->size());
                index_ = idx;
                channel_ = chan;
            }
        }
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to publish event");
    TODO("AR: the logging could emit an exception, violating noexcept?");
}


void
AmqpEventPublisher::publish_(const events::Event& ev,
                             const boost::shared_ptr<AmqpUris>& uris,
                             const boost::shared_ptr<AmqpExchange>& exchange,
                             const boost::shared_ptr<AmqpRoutingKey>& routing_key,
                             AmqpClient::Channel::ptr_t& chan,
                             unsigned& idx) const
{
    VERIFY(idx < uris->size());

    // a lot of copying but we don't care for now.
    events::EventMessage evmsg;
    evmsg.set_cluster_id(cluster_id_.str());
    evmsg.set_node_id(node_id_.str());
    *evmsg.mutable_event() = ev;

    std::string s;
    evmsg.SerializeToString(&s);

    LOCK_CHANNEL();

    AmqpClient::BasicMessage::ptr_t msg(AmqpClient::BasicMessage::Create(s));

    const unsigned attempts = uris->size();

    for (unsigned i = 0; i < attempts; ++i)
    {
        const AmqpUri& uri(uris->at(idx));

        try
        {
            if (chan == nullptr)
            {
                LOG_INFO("Trying to establish channel with " << uri);
                chan = AmqpClient::Channel::CreateFromUri(uri.str(),
                                                          max_frame_size);
                VERIFY(chan);
                enable_keepalive(*chan);
            }

            chan->BasicPublish(*exchange,
                               *routing_key,
                               msg);

            return;
        }
        CATCH_STD_ALL_EWHAT({
                LOG_WARN("Failed to publish event " <<
                         ev.GetDescriptor()->full_name() <<
                         " to " << uri << ": " << EWHAT);
                chan = nullptr;
                idx = (idx + 1) % attempts;
            });
    }

    LOG_ERROR("Failed to publish event " << ev.GetDescriptor()->full_name() <<
              " - dropping it");
}

}
