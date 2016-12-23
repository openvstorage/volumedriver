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

#include "../LocORemClient.h"
#include "../LocORemConnection.h"
#include "../LocORemServer.h"
#include "../Logging.h"
#include "../ScopeExit.h"
#include "../SharedMemoryRegion.h"
#include "../System.h"
#include "../wall_timer.h"

#include <gtest/gtest.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <boost/filesystem.hpp>
#include <boost/range.hpp>
#include <boost/thread.hpp>

namespace youtilstest
{

namespace ba = boost::asio;
namespace bs = boost::system;
namespace fs = boost::filesystem;
namespace yt = youtils;

class LocORemTest
    : public ::testing::TestWithParam<ForceRemote>
{
protected:
    using Seconds = std::chrono::seconds;
    using MaybeDelay = boost::optional<Seconds>;

    LocORemTest()
        : hdr_size_(yt::System::get_env_with_default<uint32_t>("LOCOREM_HEADER_SIZE",
                                                               64))
        , msg_size_(yt::System::get_env_with_default<uint32_t>("LOCOREM_MESSAGE_SIZE",
                                                               0))
        , iterations_(yt::System::get_env_with_default<uint32_t>("LOCOREM_MESSAGE_COUNT",
                                                                 10000))
        , nthreads_(yt::System::get_env_with_default<uint32_t>("LOCOREM_SERVER_THREADS",
                                                               boost::thread::hardware_concurrency()))
        , nclients_(yt::System::get_env_with_default<uint16_t>("LOCOREM_CLIENTS",
                                                              1))
        , addr_(yt::System::get_env_with_default<std::string>("LOCOREM_ADDRESS",
                                                              "127.0.0.1"))
        , port_(yt::System::get_env_with_default<uint16_t>("LOCOREM_PORT",
                                                           23232))
        , hdr_bytes_(0)
        , msg_bytes_(0)
    {
        EXPECT_LT(0U, hdr_size_) << "Fix your test";
    }

    std::unique_ptr<yt::LocORemClient>
    make_client(const Seconds& timeout = Seconds(1))
    {
        return std::make_unique<yt::LocORemClient>(addr_,
                                                   port_,
                                                   timeout,
                                                   GetParam());
    }

    struct ServerParameters
    {
        ServerParameters()
        {}

        ~ServerParameters() = default;

        ServerParameters(const ServerParameters&) = default;

        ServerParameters&
        operator=(const ServerParameters&) = default;

#define PARAM(typ, name)                         \
        ServerParameters&                        \
        name(const typ& val)                     \
        {                                        \
            name ## _ = val;                     \
                return *this;                    \
        }                                        \
                                                 \
        typ name ## _

        PARAM(std::shared_ptr<yt::SharedMemoryRegion>, shmem) = nullptr;
        PARAM(MaybeDelay, accept_delay) = boost::none;
        PARAM(MaybeDelay, read_delay) = boost::none;
        PARAM(MaybeDelay, write_delay) = boost::none;
        PARAM(Seconds, timeout) = Seconds(1);

#undef PARAM
    };

    using ServerParametersPtr = std::shared_ptr<ServerParameters>;

    std::unique_ptr<yt::LocORemServer>
    make_server(const ServerParameters& p = ServerParameters())
    {
        const auto params(std::make_shared<ServerParameters>(p));

        auto unix_fun([params,
                       this](yt::LocORemUnixConnection& conn)
                      {
                          echo_server_accept_(conn,
                                              params);
                      });

        auto tcp_fun([params,
                      this](yt::LocORemTcpConnection& conn)
                     {
                         echo_server_accept_(conn,
                                             params);
                     });

        return std::make_unique<yt::LocORemServer>(addr_,
                                                   port_,
                                                   std::move(unix_fun),
                                                   std::move(tcp_fun),
                                                   nthreads_);
    }

    DECLARE_LOGGER("LocORemTest");

    const uint32_t hdr_size_;
    const uint32_t msg_size_;
    const uint64_t iterations_;
    uint32_t nthreads_;
    uint32_t nclients_;
    const std::string addr_;
    const uint16_t port_;

    std::atomic<uint64_t> hdr_bytes_;
    std::atomic<uint64_t> msg_bytes_;

private:
    void
    maybe_take_a_nap_(const MaybeDelay& maybe_delay)
    {
        if (maybe_delay)
        {
            LOG_TRACE("taking a nap of " << maybe_delay->count() << " seconds");
            std::this_thread::sleep_for(*maybe_delay);
            LOG_TRACE("back from taking a nap");
        }
    }

    using MessagePtr = std::shared_ptr<std::vector<char>>;
    using HeaderPtr = MessagePtr;

    template<typename C>
    void
    echo_server_accept_(C& conn,
                        ServerParametersPtr params)
    {
        LOG_TRACE("new connection, starting to echo");

        auto hdr(std::make_shared<std::vector<char>>(hdr_size_, 'H'));

        auto msg(msg_size_ ?
                 std::make_shared<std::vector<char>>(msg_size_, 'S') :
                 nullptr);

        maybe_take_a_nap_(params->accept_delay_);

        echo_server_read_header_(conn,
                                 params,
                                 hdr,
                                 msg);
    }

    template<typename C>
    void
    echo_server_read_header_(C& conn,
                             ServerParametersPtr params,
                             HeaderPtr hdr,
                             MessagePtr msg)
    {
        conn.async_read(ba::buffer(*hdr),
                        [params,
                         hdr,
                         msg,
                         this]
                        (C& conn)
                        {
                            LOG_TRACE("read header completion");
                            hdr_bytes_.fetch_add(hdr->size());
                            echo_server_read_message_(conn,
                                                      params,
                                                      hdr,
                                                      msg);
                        },
                        params->timeout_);
    }

    template<typename C>
    void
    echo_server_read_message_(C& conn,
                              ServerParametersPtr params,
                              HeaderPtr hdr,
                              MessagePtr msg)
    {
        if (msg)
        {
            if (params->shmem_)
            {
                LOG_TRACE("reading req data from shmem");

                ASSERT_LE(msg->size(),
                          params->shmem_->size());
                memcpy(msg->data(),
                       params->shmem_->address(),
                       msg->size());

                msg_bytes_.fetch_add(msg->size());

                echo_server_write_(conn,
                                   params,
                                   hdr,
                                   msg);
            }
            else
            {
                LOG_TRACE("reading req data from socket");

                conn.async_read(ba::buffer(*msg),
                                [hdr,
                                 msg,
                                 params,
                                 this](C& conn)
                                {
                                    LOG_TRACE("read message completion");
                                    msg_bytes_.fetch_add(msg->size());

                                    echo_server_write_(conn,
                                                       params,
                                                       hdr,
                                                       msg);
                                },
                                params->timeout_);
            }
        }
        else
        {
            echo_server_write_(conn,
                               params,
                               hdr,
                               msg);
        }
    }

    template<typename C>
    void
    echo_server_write_(C& conn,
                       ServerParametersPtr params,
                       HeaderPtr hdr,
                       MessagePtr msg)
    {
        maybe_take_a_nap_(params->read_delay_);

        auto f([hdr,
                msg,
                params,
                this](C& conn)
               {
                   LOG_TRACE("write completion");
                   maybe_take_a_nap_(params->write_delay_);

                   echo_server_read_header_(conn,
                                            params,
                                            hdr,
                                            msg);
               });

        if (msg)
        {
            if (params->shmem_)
            {
                LOG_TRACE("writing rsp data to shmem");

                memcpy(params->shmem_->address(),
                       msg->data(),
                       msg->size());

                conn.async_write(ba::buffer(*hdr),
                                 std::move(f),
                                 params->timeout_);
            }
            else
            {
                LOG_TRACE("writing rsp data to socket");

                boost::array<ba::const_buffer, 2> bufs{{
                        ba::buffer(*hdr),
                        ba::buffer(*msg)
                }};

                conn.async_write(bufs,
                                 std::move(f),
                                 params->timeout_);
            }
        }
        else
        {
            conn.async_write(ba::buffer(*hdr),
                             std::move(f),
                             params->timeout_);
        }
    }
};

TEST_P(LocORemTest, echo)
{
    std::shared_ptr<yt::SharedMemoryRegion> shmem;
    if (msg_size_ and GetParam() == ForceRemote::F)
    {
        shmem = std::make_shared<yt::SharedMemoryRegion>(msg_size_);
    }

    auto server(make_server(ServerParameters().shmem(shmem)));

    yt::wall_timer w;
    const Seconds timeout(1);

    std::vector<std::shared_future<void>> futures;
    futures.reserve(nclients_);

    for (size_t i = 0; i < nclients_; ++i)
    {
        auto fun([&]
                 {
                     auto client(make_client());

                     std::vector<uint8_t> hbuf(hdr_size_, 'h');
                     std::vector<uint8_t> mbuf(msg_size_, 'm');

                     for (uint32_t i = 0; i < iterations_; ++i)
                     {
                         if (not mbuf.empty() and not shmem)
                         {
                             LOG_TRACE("writing req data to socket");

                             const boost::array<ba::const_buffer, 2> bufs = {{
                                     ba::buffer(hbuf),
                                     ba::buffer(mbuf),
                                 }};

                             client->send(bufs,
                                          timeout);
                         }
                         else
                         {
                             if (shmem)
                             {
                                 LOG_TRACE("writing req data to shmem");

                                 memcpy(shmem->address(),
                                        mbuf.data(),
                                        mbuf.size());
                             }

                             client->send(ba::buffer(hbuf),
                                          timeout);
                         }

                         client->recv(ba::buffer(hbuf),
                                      timeout);

                         if (not mbuf.empty())
                         {
                             if (shmem)
                             {
                                 LOG_TRACE("reading rsp data from shmem");

                                 memcpy(mbuf.data(),
                                        shmem->address(),
                                        mbuf.size());
                             }
                             else
                             {
                                 LOG_TRACE("reading rsp data from socket");

                                 client->recv(ba::buffer(mbuf),
                                              timeout);
                             }
                         }
                     }
                 });

        futures.push_back(std::async(std::launch::async,
                                     std::move(fun)));
    }

    for (auto& f : futures)
    {
        f.wait();
    }

    const double t = w.elapsed();

    std::cout << iterations_ << " iterations with headers of " << hdr_size_ <<
        " and messages of " << msg_size_ << " bytes, " << nclients_ <<
        " clients took " << t << " seconds -> " <<
        (nclients_ * iterations_) / t << " IOPS" << std::endl;

    const uint64_t exp_hdr_bytes = nclients_ * iterations_ * hdr_size_;
    ASSERT_EQ(exp_hdr_bytes,
              hdr_bytes_.load());

    const uint64_t exp_msg_bytes = nclients_ * iterations_ * msg_size_;
    ASSERT_EQ(exp_msg_bytes,
              msg_bytes_.load());
}

TEST_P(LocORemTest, no_server)
{
    EXPECT_THROW(make_client(),
                 bs::system_error);
}

TEST_P(LocORemTest, server_gone)
{
    auto server(make_server());
    auto client(make_client());

    const std::vector<uint8_t> rbuf(hdr_size_, 'Q');
    std::vector<uint8_t> wbuf(rbuf.size());

    const Seconds timeout(1);

    client->send(ba::buffer(rbuf),
                 timeout);

    client->recv(ba::buffer(wbuf),
                 timeout);

    server.reset();

    if (GetParam() == ForceRemote::T)
    {
        // Doesn't throw but only fills up the TCP buffer
        client->send(ba::buffer(rbuf),
                     timeout);
    }
    else
    {
        EXPECT_THROW(client->send(ba::buffer(rbuf),
                                  timeout),
                     bs::system_error);
    }

    EXPECT_THROW(client->recv(ba::buffer(wbuf),
                              timeout),
                 bs::system_error);
}

TEST_P(LocORemTest, client_gone)
{
    const Seconds delay(2);

    auto server(make_server(ServerParameters()
                            .read_delay(delay)));

    const Seconds timeout(1);

    {
        auto client(make_client());

        const std::vector<uint8_t> rbuf(hdr_size_, 'Q');

        client->send(ba::buffer(rbuf),
                     timeout);
    }

    std::this_thread::sleep_for(delay);

    auto client(make_client());

    const std::vector<uint8_t> rbuf(hdr_size_, 'X');
    std::vector<uint8_t> wbuf(rbuf.size());

    client->send(ba::buffer(rbuf),
                 timeout);

    client->recv(ba::buffer(wbuf),
                 Seconds(delay.count() * 2));
}

TEST_P(LocORemTest, client_recv_timeout)
{
    auto server(make_server(ServerParameters().read_delay(Seconds(3))));
    auto client(make_client());

    const Seconds timeout(1);
    const std::vector<uint8_t> rbuf(hdr_size_, 'Q');
    std::vector<uint8_t> wbuf(rbuf.size());

    client->send(ba::buffer(rbuf),
                 timeout);

    EXPECT_THROW(client->recv(ba::buffer(wbuf),
                              timeout),
                 bs::system_error);
}

// cf. OVS-2102
// * is there are better way to figure out the number of open fd's?
// * boost::asio could use something else than epoll so the number of fd's isn't
//   set in stone?
TODO("AR: make this test less brittle");

TEST_P(LocORemTest, accept_out_of_fds)
{
    nthreads_ = 1;

    auto server(make_server());
    auto client(make_client());

    const Seconds timeout(1);
    const std::vector<uint8_t> rbuf(hdr_size_, 'Q');
    std::vector<uint8_t> wbuf(rbuf.size());

    client->send(ba::buffer(rbuf),
                 timeout);

    client->recv(ba::buffer(wbuf),
                 timeout);

    const int res = std::distance(fs::directory_iterator("/proc/self/fd"),
                                  fs::directory_iterator());
    ASSERT_LE(0,
              res);

    // The gentle reader will undoubtedly be aware that `res' includes the
    // fd necessary to read the directory entries. We want our test to trigger
    // a failure in accept() so we need room for more fd's:
    // * socket
    // * epoll
    // * eventfd
    // * timerfd
    unsigned openfds = res + 3;

    rlimit cur;
    ASSERT_EQ(0,
              getrlimit(RLIMIT_NOFILE,
                        &cur));

    {
        const rlimit tmp = { openfds,
                             cur.rlim_max };
        ASSERT_EQ(0,
                  setrlimit(RLIMIT_NOFILE,
                            &tmp));

        auto exit(yt::make_scope_exit([&]
                                      {
                                          setrlimit(RLIMIT_NOFILE,
                                                    &cur);
                                      }));

        // Interesting: connect succeeds and we can even send data. Only once we read
        // we'll bump into an error.
        auto client2(make_client());

        client2->send(ba::buffer(rbuf),
                      timeout);
        ASSERT_THROW(client2->recv(ba::buffer(wbuf),
                                   timeout),
                     std::exception);
    }
}

TEST_P(LocORemTest, DISABLED_server)
{
    auto server(make_server());
    std::this_thread::sleep_for(std::chrono::minutes(1440));
}

TEST_P(LocORemTest, DISABLED_client)
{
    const auto client(make_client());

    const std::vector<uint8_t> rbuf(hdr_size_, 'Q');
    std::vector<uint8_t> wbuf(rbuf.size());

    const auto timeout(Seconds(1));

    yt::wall_timer w;

    for (uint32_t i = 0; i < iterations_; ++i)
    {
        client->send(ba::buffer(rbuf),
                     timeout);

        client->recv(ba::buffer(wbuf),
                     timeout);
    }

    const double t = w.elapsed();

    std::cout << iterations_ << " iterations with messages of " << msg_size_ <<
        " bytes took " << t << " seconds -> " << iterations_ / t <<
        " IOPS" << std::endl;

    EXPECT_EQ(0, memcmp(rbuf.data(),
                        wbuf.data(),
                        wbuf.size()));
}

INSTANTIATE_TEST_CASE_P(LocORemTests,
                        LocORemTest,
                        ::testing::Values(ForceRemote::F,
                                          ForceRemote::T));
}
