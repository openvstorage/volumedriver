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

#include <boost/scope_exit.hpp>
#include <boost/thread.hpp>

#include <cppzmq/zmq.hpp>

#include <youtils/Logging.h>
#include <youtils/System.h>
#include <youtils/TestBase.h>
#include <youtils/UUID.h>
#include <youtils/wall_timer.h>

#include "../ZUtils.h"
#include "../ZWorkerPool.h"

namespace volumedriverfstest
{

namespace vfs = volumedriverfs;
namespace yt = youtils;

class ZWorkerPoolTest
    : public youtilstest::TestBase
{
protected:
    ZWorkerPoolTest()
        : ztx_(0)
        , addr_("inproc://" + yt::UUID().str())
    {}

    vfs::ZWorkerPool::MessageParts
    sleeping_worker(vfs::ZWorkerPool::MessageParts&& parts_in)
    {
        EXPECT_EQ(1U, parts_in.size());
        EXPECT_EQ(sizeof(uint32_t), parts_in[0].size());

        uint32_t timeout_secs = *static_cast<uint32_t*>(parts_in[0].data());

        LOG_INFO(boost::this_thread::get_id() << " going to sleep for " <<
                 timeout_secs << " seconds");

        boost::this_thread::sleep_for(boost::chrono::seconds(timeout_secs));

        std::vector<zmq::message_t> parts_out;
        parts_out.reserve(1);
        parts_out.emplace_back(std::move(parts_in[0]));

        return parts_out;
    }

    zmq::socket_t
    create_client_socket()
    {
        zmq::socket_t zock(ztx_, ZMQ_REQ);
        vfs::ZUtils::socket_no_linger(zock);
        zock.connect(addr_.c_str());

        return zock;
    }

    DECLARE_LOGGER("ZWorkerPoolTest");

    zmq::context_t ztx_;
    const std::string addr_;
};

TEST_F(ZWorkerPoolTest, construction)
{
    EXPECT_THROW(vfs::ZWorkerPool("MinSizeZero",
                                  ztx_,
                                  addr_,
                                  [&](std::vector<zmq::message_t>&& parts)
                                  -> std::vector<zmq::message_t>
                                  {
                                      return std::move(parts);
                                  },
                                  0,
                                  1),
                 std::exception);

    EXPECT_THROW(vfs::ZWorkerPool("MaxLessThanMin",
                                  ztx_,
                                  addr_,
                                  [&](std::vector<zmq::message_t>&& parts)
                                  -> std::vector<zmq::message_t>
                                  {
                                      return std::move(parts);
                                  },
                                  2,
                                  1),
                 std::exception);

    vfs::ZWorkerPool zwpool("TestZWorkerPool1",
                            ztx_,
                            addr_,
                            [&](std::vector<zmq::message_t>&& parts)
                            -> std::vector<zmq::message_t>
                            {
                                return std::move(parts);
                            },
                            1,
                            1);
    ztx_.close();
}

TEST_F(ZWorkerPoolTest, address_conflict)
{
    const uint16_t min_workers = 1;
    const uint16_t max_workers = 1;

    vfs::ZWorkerPool zwpool("TestZWorkerPool1",
                            ztx_,
                            addr_,
                            [&](std::vector<zmq::message_t>&& parts)
                            -> std::vector<zmq::message_t>
                            {
                                return std::move(parts);
                            },
                            min_workers,
                            max_workers);

    {
        BOOST_SCOPE_EXIT((&ztx_))
        {
            ztx_.close();
        }
        BOOST_SCOPE_EXIT_END;

        EXPECT_THROW(vfs::ZWorkerPool("TestZWorkerPool2",
                                      ztx_,
                                      addr_,
                                      [&](std::vector<zmq::message_t>&& parts)
                                      -> std::vector<zmq::message_t>
                                      {
                                          return std::move(parts);
                                      },
                                      min_workers,
                                      max_workers),
                     std::exception);
    }
}

TEST_F(ZWorkerPoolTest, basics)
{
    const uint16_t min_workers = 2;
    const uint16_t max_workers = 4;

    auto fun([&](std::vector<zmq::message_t>&& parts_in) -> std::vector<zmq::message_t>
             {
                 return sleeping_worker(std::move(parts_in));
             });

    vfs::ZWorkerPool zwpool("TestZWorkerPool",
                            ztx_,
                            addr_,
                            std::move(fun),
                            min_workers,
                            max_workers);
    {
        BOOST_SCOPE_EXIT((&ztx_))
        {
            ztx_.close();
        }
        BOOST_SCOPE_EXIT_END;

        std::vector<zmq::socket_t> socks;
        socks.reserve(max_workers);

        const uint32_t timeout_secs = 10;

        yt::wall_timer wt;

        LOG_TRACE("starting to send");

        for (uint16_t i = 0; i < max_workers; ++i)
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
}

TEST_F(ZWorkerPoolTest, maxed_out)
{
    const uint16_t min_workers = 1;
    const uint16_t max_workers = 1;

    auto fun([&](std::vector<zmq::message_t>&& parts_in) -> std::vector<zmq::message_t>
             {
                 return sleeping_worker(std::move(parts_in));
             });

    vfs::ZWorkerPool zwpool("TestZWorkerPool",
                            ztx_,
                            addr_,
                            std::move(fun),
                            min_workers,
                            max_workers);

    {
        BOOST_SCOPE_EXIT((&ztx_))
        {
            ztx_.close();
        }
        BOOST_SCOPE_EXIT_END;


        const uint32_t timeout_secs = 10;

        std::vector<zmq::socket_t> socks;
        socks.reserve(max_workers + 1);

        yt::wall_timer wt;

        for (uint16_t i = 0; i < max_workers + 1; ++i)
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
}

TEST_F(ZWorkerPoolTest, exceptional_work)
{
    const uint16_t min_workers = 1;
    const uint16_t max_workers = 1;

    unsigned count = 0;

    std::promise<bool> promise;
    std::future<bool> future(promise.get_future());

    auto fun([&](std::vector<zmq::message_t>&& parts_in) -> std::vector<zmq::message_t>
             {
                 if (count++ == 0)
                 {
                     LOG_TRACE("unblocking");
                     promise.set_value(true);
                     LOG_INFO("throwing");
                     throw fungi::IOException("Just pretending");
                 }

                 return std::move(parts_in);
             });

    vfs::ZWorkerPool zwpool("TestZWorkerPool",
                            ztx_,
                            addr_,
                            std::move(fun),
                            min_workers,
                            max_workers);

    {
        BOOST_SCOPE_EXIT((&ztx_))
        {
            ztx_.close();
        }
        BOOST_SCOPE_EXIT_END;

        {
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
    }
}

TEST_F(ZWorkerPoolTest, stress)
{
    const uint16_t min_workers = 2;
    const uint16_t max_workers = 4;

    const size_t iterations = yt::System::get_env_with_default("ITERATIONS", 1000UL);

    auto fun([&](std::vector<zmq::message_t>&& parts_in) -> std::vector<zmq::message_t>
             {
                 return std::move(parts_in);
             });

    vfs::ZWorkerPool zwpool("TestZWorkerPool",
                            ztx_,
                            addr_,
                            std::move(fun),
                            min_workers,
                            max_workers);
    {
        BOOST_SCOPE_EXIT((&ztx_))
        {
            ztx_.close();
        }
        BOOST_SCOPE_EXIT_END;

        std::vector<zmq::socket_t> socks;
        socks.reserve(max_workers);

        for (uint16_t i = 0; i < max_workers; ++i)
        {
            socks.emplace_back(create_client_socket());
        }

        for (size_t i = 0; i < iterations; ++i)
        {
            for (uint16_t j = 0; j < max_workers; ++j)
            {
                zmq::message_t msg1(sizeof(i));
                *static_cast<size_t*>(msg1.data()) = i;
                socks[j].send(msg1, ZMQ_SNDMORE);

                zmq::message_t msg2(sizeof(j));
                *static_cast<uint16_t*>(msg2.data()) = j;
                socks[j].send(msg2, 0);
            }

            for (uint16_t j = 0; j < max_workers; ++j)
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
}

}
