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

#ifndef VFS_ZMQ_UTILS_H_
#define VFS_ZMQ_UTILS_H_

#include "Protocol.h"
#include <cppzmq/zmq.hpp>

#include <google/protobuf/message.h>
#include <boost/type_traits/add_reference.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/utility/enable_if.hpp>

#include <youtils/BooleanEnum.h>
#include <youtils/Catchers.h>
#include <youtils/IOException.h>
#include <youtils/Logging.h>

namespace volumedriverfs
{

BOOLEAN_ENUM(MoreMessageParts);

MAKE_EXCEPTION(ProtocolError, fungi::IOException);

// this needs to be revisited to get rid of a few unnecessary copy operations

template<typename T, typename Enable = void>
struct ZTraits;

template<typename T>
struct ZTraits<T,
               typename boost::enable_if<boost::is_convertible<T*,
                                                               google::protobuf::Message*>>::type>
{
    static zmq::message_t
    serialize(const T& t)
    {
        std::string s(t.SerializeAsString());
        zmq::message_t msg(s.size());
        memcpy(msg.data(), s.c_str(), s.size());
        return msg;
    }

    static void
    deserialize(const zmq::message_t& msg,
                             T& t)
    {
        t.ParseFromArray(msg.data(), msg.size());
    }
};

template<>
struct ZTraits<vfsprotocol::RequestType>
{
    static zmq::message_t
    serialize(const vfsprotocol::RequestType& t)
    {
        zmq::message_t msg(sizeof(t));
        memcpy(msg.data(), &t, sizeof(t));
        return msg;
    }

    static void
    deserialize(const zmq::message_t& msg,
                             vfsprotocol::RequestType& t)
    {
        if (sizeof(t) != msg.size())
        {
            throw ProtocolError("failed to deserialize RequestType");
        }

        memcpy(&t, msg.data(), sizeof(t));
    }
};

template<>
struct ZTraits<vfsprotocol::ResponseType>
{
    static zmq::message_t
    serialize(const vfsprotocol::ResponseType& t)
    {
        zmq::message_t msg(sizeof(t));
        memcpy(msg.data(), &t, sizeof(t));
        return msg;
    }

    static void
    deserialize(const zmq::message_t& msg,
                             vfsprotocol::ResponseType& t)
    {
        if (sizeof(t) != msg.size())
        {
            throw ProtocolError("failed to deserialize ResponseType");
        }

        memcpy(&t, msg.data(), sizeof(t));
    }
};

template<>
struct ZTraits<vfsprotocol::Tag>
{
    static zmq::message_t
    serialize(const vfsprotocol::Tag& t)
    {
        zmq::message_t msg(sizeof(t));
        memcpy(msg.data(), &t, sizeof(t));
        return msg;
    }

    static void
    deserialize(const zmq::message_t& msg,
                             vfsprotocol::Tag& t)
    {
        if (sizeof(t) != msg.size())
        {
            throw ProtocolError("failed to deserialize Tag");
        }

        memcpy(&t, msg.data(), sizeof(t));
    }
};

struct ZUtils
{
    template<typename T, typename Traits = ZTraits<T> >
    static zmq::message_t
    serialize_to_message(const T& t)
    {
        return Traits::serialize(t);
    }

    template<typename T, typename Traits = ZTraits<T> >
    static void
    deserialize_from_message(const zmq::message_t& msg, T& t)
    {
        Traits::deserialize(msg, t);
    }

    template<typename M, typename Traits = ZTraits<M> >
    static void
    serialize_to_socket(zmq::socket_t& sock,
                        const M& msg,
                        MoreMessageParts more)
    {
        zmq::message_t msg_part(serialize_to_message<M, Traits>(msg));
        LOG_TRACE("sending a message of " << msg_part.size() << " bytes");
        sock.send(msg_part, T(more) ? ZMQ_SNDMORE : 0);
    }

    template<typename T, typename Traits = ZTraits<T> >
    static void
    deserialize_from_socket(zmq::socket_t& sock, T& t)
    {
        zmq::message_t msg;
        sock.recv(&msg);
        LOG_TRACE("received a message of " << msg.size() << " bytes");
        deserialize_from_message<T, Traits>(msg, t);
    }

    static bool
    more_message_parts(zmq::socket_t& sock)
    {
        int more;
        size_t more_size = sizeof(more);
        sock.getsockopt(ZMQ_RCVMORE, &more, &more_size);
        return more;
    }

    static void
    drop_remaining_message_parts(zmq::socket_t& sock)
    {
        while (more_message_parts(sock))
        {
            zmq::message_t msg;
            sock.recv(&msg);
        }
    }

    // Throw away any pending messages immediately if we're exiting.
    static void
    socket_no_linger(zmq::socket_t& zock)
    {
        const int linger = 0;
        zock.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
    }

private:
    DECLARE_LOGGER("ZUtils");
};

#define ZEXPECT_NOTHING_MORE(sock)                                      \
    if (volumedriverfs::ZUtils::more_message_parts(sock))               \
    {                                                                   \
        throw volumedriverfs::ProtocolError("Expected no more message parts but there is at least one"); \
    }

#define ZEXPECT_MORE(sock, partstr)                                     \
    if (not volumedriverfs::ZUtils::more_message_parts(sock))           \
    {                                                                   \
        throw volumedriverfs::ProtocolError("Expected " partstr ", got nothing"); \
    }

#define ZEXPECT_MESSAGE_SIZE(exp, got, partstr)                         \
    if (exp != got)                                                     \
    {                                                                   \
        throw volumedriverfs::ProtocolError("Expected " partstr ", got something of different size"); \
    }

}

#endif // !VFS_ZMQ_UTILS_H_
