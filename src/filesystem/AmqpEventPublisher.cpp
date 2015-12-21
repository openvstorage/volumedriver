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

#include <amqp.h>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/System.h>

namespace volumedriverfs
{

#define LOCK()                                  \
    std::lock_guard<lock_type> gl__(lock_)

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
check_uris(const ip::PARAMETER_TYPE(events_amqp_uris)& uris)
{
    std::for_each(uris.value().begin(),
                  uris.value().end(),
                  check_uri);
}

void
enable_keepalive(AmqpClient::Channel& channel)
{
    int sock = channel.SockFd();
    VERIFY(sock >= 0);

    auto set([&](const char* desc,
                 int level,
                 int optname,
                 int val)
             {
                 int ret = ::setsockopt(sock,
                                        level,
                                        optname,
                                        &val,
                                        sizeof(val));
                 if (ret < 0)
                 {
                     ret = errno;
                     LOG_ERROR("Failed to set " << desc << " on socket " << sock <<
                               ": " << strerror(errno));
                     throw fungi::IOException("Failed to set socket option",
                                              desc);
                 }
             });

    set("KeepAlive",
        SOL_SOCKET,
        SO_KEEPALIVE,
        1);

    static const int keep_cnt =
        yt::System::get_env_with_default("AMQP_CLIENT_KEEPCNT",
                                         5);

    set("keepalive probes",
        SOL_TCP,
        TCP_KEEPCNT,
        keep_cnt);

    static const int keep_idle =
        yt::System::get_env_with_default("AMQP_CLIENT_KEEPIDLE",
                                         60);

    set("keepalive time",
        SOL_TCP,
        TCP_KEEPIDLE,
        keep_idle);

    static const int keep_intvl =
        yt::System::get_env_with_default("AMQP_CLIENT_KEEPINTVL",
                                         60);

    set("keepalive interval",
        SOL_TCP,
        TCP_KEEPINTVL,
        keep_intvl);
}

}

AmqpEventPublisher::AmqpEventPublisher(const ClusterId& cluster_id,
                                       const NodeId& node_id,
                                       const bpt::ptree& pt,
                                       const RegisterComponent registrate)
    : VolumeDriverComponent(registrate, pt)
    , index_(0)
    , events_amqp_uris(pt)
    , events_amqp_exchange(pt)
    , events_amqp_routing_key(pt)
    , cluster_id_(cluster_id)
    , node_id_(node_id)
{
    check_uris(events_amqp_uris);

    if (not enabled_())
    {
        LOG_WARN("No AMQP URI specified - event notifications will be discarded");
    }
}

bool
AmqpEventPublisher::enabled() const
{
    LOCK();
    return enabled_();
}

bool
AmqpEventPublisher::enabled_() const
{
    return not events_amqp_uris.value().empty();
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
do_update(T& t,
          const bpt::ptree& pt,
          yt::UpdateReport& urep,
          bool& updated)
{
    const T u(pt);

    if (u.value() != t.value())
    {
        updated = true;
    }

    t.update(pt,
             urep);
}

}

void
AmqpEventPublisher::update(const boost::property_tree::ptree& pt,
                           yt::UpdateReport& urep)
{
    LOCK();

    bool updated = false;

    do_update(events_amqp_uris,
              pt,
              urep,
              updated);

    do_update(events_amqp_exchange,
              pt,
              urep,
              updated);

    do_update(events_amqp_routing_key,
              pt,
              urep,
              updated);

    if (updated)
    {
        index_ = 0;
        channel_.reset();
    }
}

void
AmqpEventPublisher::persist(boost::property_tree::ptree& pt,
                            const ReportDefault report_default) const
{
    LOCK();

#define P(var)                                  \
    (var).persist(pt, report_default)

    P(events_amqp_uris);
    P(events_amqp_exchange);
    P(events_amqp_routing_key);

#undef P
}

bool
AmqpEventPublisher::checkConfig(const boost::property_tree::ptree& pt,
                                yt::ConfigurationReport& crep) const
{
    const decltype(events_amqp_uris) uris(pt);
    try
    {
        check_uris(uris);
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
        LOCK();

        if (enabled_())
        {
            // a lot of copying but we don't care for now.
            events::EventMessage evmsg;
            evmsg.set_cluster_id(cluster_id_.str());
            evmsg.set_node_id(node_id_.str());

            *evmsg.mutable_event() = ev;
            std::string s;
            evmsg.SerializeToString(&s);
            auto msg(AmqpClient::BasicMessage::Create(s));

            const unsigned attempts = events_amqp_uris.value().size();
            for (unsigned i = 0; i < attempts; ++i)
            {
                const AmqpUri uri = events_amqp_uris.value()[index_];

                try
                {
                    if (channel_ == nullptr)
                    {
                        LOG_INFO("Trying to establish channel with " << uri);
                        channel_ = AmqpClient::Channel::CreateFromUri(uri.str(),
                                                                      max_frame_size);
                        VERIFY(channel_);
                        enable_keepalive(*channel_);
                    }

                    channel_->BasicPublish(events_amqp_exchange.value(),
                                           events_amqp_routing_key.value(),
                                           msg);

                    LOG_TRACE("Published event " << ev.GetDescriptor()->full_name() <<
                              " to " << uri);
                    return;
                }
                CATCH_STD_ALL_EWHAT({
                        LOG_WARN("Failed to publish event " <<
                                 ev.GetDescriptor()->full_name() <<
                                 " to " << uri << ": " << EWHAT);
                        channel_ = nullptr;
                        index_ = (index_ + 1) % attempts;
                    });
            }

            LOG_ERROR("Failed to publish event " << ev.GetDescriptor()->full_name() <<
                      " - dropping it");
        }
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to publish event");
    TODO("AR: the logging could emit an exception, violating noexcept?");
}

}
