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

#include "../ZUtils.h"
#include "../ZWorkerPool.h"

#include <boost/thread.hpp>

#include <cppzmq/zmq.hpp>

#include <gtest/gtest.h>

#include <youtils/Logging.h>
#include <youtils/System.h>
#include <youtils/UUID.h>
#include <youtils/wall_timer.h>

namespace volumedriverfstest
{

namespace vfs = volumedriverfs;
namespace yt = youtils;
using namespace std::literals::string_literals;

namespace
{

vfs::ZWorkerPool::WorkerFun
reflect_work([](vfs::ZWorkerPool::MessageParts parts) -> vfs::ZWorkerPool::MessageParts
             {
                 return parts;
             });

vfs::ZWorkerPool::DispatchFun
dispatch_to_thread([](const vfs::ZWorkerPool::MessageParts&) -> yt::DeferExecution
                   {
                       return yt::DeferExecution::T;
                   });
}

class ZWorkerPoolTest
    : public testing::Test
{
protected:
    ZWorkerPoolTest()
        : ztx_(0)
        , uri_(yt::Uri().scheme("inproc"s).host(yt::UUID().str()))
    {}

    vfs::ZWorkerPool::MessageParts
    sleeping_worker(vfs::ZWorkerPool::MessageParts parts_in)
    {
        EXPECT_EQ(1U, parts_in.size());
        EXPECT_EQ(sizeof(uint32_t), parts_in[0].size());

        uint32_t timeout_secs = *static_cast<uint32_t*>(parts_in[0].data());

        LOG_INFO(boost::this_thread::get_id() << " going to sleep for " <<
                 timeout_secs << " seconds");

        boost::this_thread::sleep_for(boost::chrono::seconds(timeout_secs));

        vfs::ZWorkerPool::MessageParts parts_out;
        parts_out.reserve(1);
        parts_out.emplace_back(std::move(parts_in[0]));

        return parts_out;
    }

    zmq::socket_t
    create_client_socket()
    {
        zmq::socket_t zock(ztx_, ZMQ_REQ);
        vfs::ZUtils::socket_no_linger(zock);
        zock.connect(boost::lexical_cast<std::string>(uri_).c_str());

        return zock;
    }

    DECLARE_LOGGER("ZWorkerPoolTest");

    zmq::context_t ztx_;
    const youtils::Uri uri_;
};

TEST_F(ZWorkerPoolTest, construction)
{
    EXPECT_THROW(vfs::ZWorkerPool("MinSizeZero",
                                  ztx_,
                                  uri_,
                                  0,
                                  reflect_work,
                                  dispatch_to_thread),
                 std::exception);

    vfs::ZWorkerPool zwpool("TestZWorkerPool1",
                            ztx_,
                            uri_,
                            1,
                            reflect_work,
                            dispatch_to_thread);
}

TEST_F(ZWorkerPoolTest, address_conflict)
{
    vfs::ZWorkerPool zwpool("TestZWorkerPool1",
                            ztx_,
                            uri_,
                            1,
                            reflect_work,
                            dispatch_to_thread);

    EXPECT_THROW(vfs::ZWorkerPool("TestZWorkerPool2",
                                  ztx_,
                                  uri_,
                                  1,
                                  reflect_work,
                                  dispatch_to_thread),
                 std::exception);
}

TEST_F(ZWorkerPoolTest, basics)
{
    const uint16_t num_workers = 4;

    auto fun([&](vfs::ZWorkerPool::MessageParts parts_in) -> vfs::ZWorkerPool::MessageParts
             {
                 return sleeping_worker(std::move(parts_in));
             });

    vfs::ZWorkerPool zwpool("TestZWorkerPool",
                            ztx_,
                            uri_,
                            num_workers,
                            std::move(fun),
                            dispatch_to_thread);

    std::vector<zmq::socket_t> socks;
    socks.reserve(num_workers);

    const uint32_t timeout_secs = 2;
    yt::wall_timer wt;

    LOG_TRACE("starting to send");

    for (uint16_t i = 0; i < num_workers; ++i)
    {
        LOG_TRACE("sending on sock " << i);

        zmq::socket_t sock(create_client_socket());
        zmq::message_t msg(sizeof(timeout_secs));
        *static_cast<uint32_t*>(msg.data()) = timeout_secs;

        sock.send(msg, 0);
        socks.emplace_back(std::move(sock));
    }

    LOG_TRACE("sending done, starting to receive");

    for (auto& s : socks)
    {
        zmq::message_t msg;
        s.recv(&msg);

        ASSERT_FALSE(vfs::ZUtils::more_message_parts(s));
    }

    LOG_TRACE("receiving done");

    ASSERT_LT(timeout_secs, wt.elapsed());
    ASSERT_GT(timeout_secs * 2, wt.elapsed());
}

TEST_F(ZWorkerPoolTest, maxed_out)
{
    const uint16_t num_workers = 1;

    auto fun([&](vfs::ZWorkerPool::MessageParts parts_in) -> vfs::ZWorkerPool::MessageParts
             {
                 return sleeping_worker(std::move(parts_in));
             });

    vfs::ZWorkerPool zwpool("TestZWorkerPool",
                            ztx_,
                            uri_,
                            num_workers,
                            std::move(fun),
                            dispatch_to_thread);

    const uint32_t timeout_secs = 2;

    std::vector<zmq::socket_t> socks;
    socks.reserve(num_workers + 1);

    yt::wall_timer wt;

    for (uint16_t i = 0; i < num_workers + 1; ++i)
    {
        LOG_TRACE("sending on sock " << i);

        zmq::socket_t sock(create_client_socket());
        zmq::message_t msg(sizeof(timeout_secs));
        *static_cast<uint32_t*>(msg.data()) = timeout_secs;

        sock.send(msg, 0);
        socks.emplace_back(std::move(sock));
    }

    for (auto& s : socks)
    {
        zmq::message_t msg;
        s.recv(&msg);

        ASSERT_FALSE(vfs::ZUtils::more_message_parts(s));
    }

    ASSERT_LT(timeout_secs * 2, wt.elapsed());
    ASSERT_GT(timeout_secs * 3, wt.elapsed());
}

TEST_F(ZWorkerPoolTest, exceptional_work)
{
    const uint16_t num_workers = 1;

    unsigned count = 0;

    std::promise<bool> promise;
    std::future<bool> future(promise.get_future());

    auto fun([&](vfs::ZWorkerPool::MessageParts parts_in) -> vfs::ZWorkerPool::MessageParts
             {
                 if (count++ == 0)
                 {
                     LOG_TRACE("unblocking");
                     promise.set_value(true);
                     LOG_INFO("throwing");
                     throw fungi::IOException("Just pretending");
                 }

                 return parts_in;
             });

    vfs::ZWorkerPool zwpool("TestZWorkerPool",
                            ztx_,
                            uri_,
                            num_workers,
                            std::move(fun),
                            dispatch_to_thread);

    const std::string cmd("echo");

    zmq::socket_t sock(create_client_socket());

    {
        zmq::message_t msg(cmd.size());
        memcpy(msg.data(), cmd.data(), msg.size());
        sock.send(msg, 0);
    }

    EXPECT_TRUE(future.get());

    zmq::socket_t sock2(create_client_socket());

    {
        zmq::message_t msg(cmd.size());
        memcpy(msg.data(), cmd.data(), msg.size());
        sock2.send(msg, 0);
    }

    {
        zmq::message_t msg;
        sock2.recv(&msg);
        const std::string s(static_cast<const char*>(msg.data()),
                            msg.size());
        EXPECT_EQ(cmd, s);
    }
}

TEST_F(ZWorkerPoolTest, stress)
{
    const uint16_t num_workers = 4;

    const size_t iterations = yt::System::get_env_with_default("ITERATIONS", 1000UL);

    vfs::ZWorkerPool zwpool("TestZWorkerPool",
                            ztx_,
                            uri_,
                            num_workers,
                            reflect_work,
                            dispatch_to_thread);

    std::vector<zmq::socket_t> socks;
    socks.reserve(num_workers);

    for (uint16_t i = 0; i < num_workers; ++i)
    {
        socks.emplace_back(create_client_socket());
    }

    for (size_t i = 0; i < iterations; ++i)
    {
        for (uint16_t j = 0; j < num_workers; ++j)
        {
            zmq::message_t msg1(sizeof(i));
            *static_cast<size_t*>(msg1.data()) = i;
            socks[j].send(msg1, ZMQ_SNDMORE);

            zmq::message_t msg2(sizeof(j));
            *static_cast<uint16_t*>(msg2.data()) = j;
            socks[j].send(msg2, 0);
        }

        for (uint16_t j = 0; j < num_workers; ++j)
        {
            zmq::message_t msg1(sizeof(i));
            socks[j].recv(&msg1);

            EXPECT_EQ(i, *static_cast<size_t*>(msg1.data()));

            zmq::message_t msg2(sizeof(j));
            socks[j].recv(&msg2);

            EXPECT_EQ(j, *static_cast<uint16_t*>(msg2.data()));
        }
    }
}

TEST_F(ZWorkerPoolTest, deferred_vs_direct)
{
    enum class RequestType
        : uint32_t
    {
        Fast,
        Slow
    };

    auto client([&](zmq::socket_t& sock, const RequestType req_type)
                {
                    {
                        zmq::message_t msg(sizeof(RequestType));
                        *reinterpret_cast<RequestType*>(msg.data()) = req_type;
                        sock.send(msg, 0);
                    }

                    zmq::message_t msg;
                    sock.recv(&msg);
                    EXPECT_EQ(req_type,
                              *reinterpret_cast<RequestType*>(msg.data()));
                });

    const size_t max_fast = 10000;
    std::atomic<size_t> num_fast(0);

    auto work([&](vfs::ZWorkerPool::MessageParts parts) -> vfs::ZWorkerPool::MessageParts
              {
                  EXPECT_EQ(1, parts.size());
                  const RequestType req_type =
                      *reinterpret_cast<RequestType*>(parts[0].data());
                  switch (req_type)
                  {
                  case RequestType::Fast:
                      ++num_fast;
                      break;
                  case RequestType::Slow:
                      {
                          zmq::socket_t sock(create_client_socket());
                          for (size_t i = 0; i < max_fast; ++i)
                          {
                              client(sock, RequestType::Fast);
                          }
                          break;
                      }
                  }
                  return parts;
              });

    auto dispatch([](const vfs::ZWorkerPool::MessageParts& parts) -> yt::DeferExecution
                  {
                      EXPECT_EQ(1, parts.size());
                      const RequestType req_type =
                          *reinterpret_cast<const RequestType*>(parts[0].data());
                      switch (req_type)
                      {
                      case RequestType::Fast:
                          return yt::DeferExecution::F;
                      case RequestType::Slow:
                          return yt::DeferExecution::T;
                      }

                      UNREACHABLE;
                  });

    const uint16_t num_workers = 1;
    vfs::ZWorkerPool pool("TestZWorkerPool",
                          ztx_,
                          uri_,
                          num_workers,
                          std::move(work),
                          std::move(dispatch));

    ASSERT_EQ(num_workers,
              pool.size());

    zmq::socket_t sock(create_client_socket());
    client(sock,
           RequestType::Slow);

    EXPECT_EQ(max_fast,
              num_fast);
}

}
