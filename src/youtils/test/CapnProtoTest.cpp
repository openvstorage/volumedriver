// Copyright (C) 2017 iNuron NV
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

#include "../Assert.h"
#include "../Catchers.h"
#include "../Logging.h"
#include "../System.h"
#include "../wall_timer.h"

PRAGMA_IGNORE_WARNING_BEGIN("-Wnon-virtual-dtor")
#include "CapnProtoTest-capnp.h"

#include <thread>

#include <boost/lexical_cast.hpp>

#include <capnp/ez-rpc.h>
#include <capnp/capability.h>
#include <capnp/message.h>
#include <capnp/serialize.h>

#include <gtest/gtest.h>

#include <kj/async.h>
#include <kj/async-io.h>

#include <sys/eventfd.h>

namespace youtilstest
{

using namespace std::literals::string_literals;
using namespace youtils;

namespace
{

// Compiled out for now as this code has the funny side effect
// of crashing gdb when loading the binary (observed with 7.{7,12}.1)
// . The serialization tests don't trigger this ... go figure.
// If you want it nevertheless this can be worked around by running
// gdb in gdb:
#ifdef ENABLE_CAPNP_RPC_TESTS

struct ServerImpl final
    : public capnp_test::Methods::Server
{
    uint64_t seq_num_;

    ServerImpl()
        : capnp_test::Methods::Server()
        , seq_num_(0)
    {}

    ~ServerImpl() = default;

    kj::Promise<void>
    nop(NopContext) override final
    {
        return kj::READY_NOW;
    }

    kj::Promise<void>
    write(WriteContext context) override final
    {
        auto params = context.getParams();
        EXPECT_TRUE(params.hasDescs());

        auto descs(params.getDescs());
        for (const auto& desc_reader : descs)
        {
            const uint64_t seq_num = desc_reader.getSeqNum();
            const capnp::Data::Reader data_reader = desc_reader.getData();
            const uint8_t* data = data_reader.begin();
            const size_t size = data_reader.size();
        }

        return kj::READY_NOW;
    }
};

class Server
{
public:
    Server(const std::string& a,
           uint16_t p)
        : host(a)
        , port(p)
        , event_fd_([]() -> int
                    {
                        int fd = ::eventfd(0, EFD_NONBLOCK);
                        THROW_WHEN(fd < 0);
                        return fd;
                    }())
        , thread_([this]
                  {
                      try
                      {
                          pthread_setname_np(pthread_self(),
                                             "capnp_test_srv");
                          run_();
                      }
                      CATCH_STD_ALL_LOG_IGNORE("Failed to run server");
                  })
    {}

    ~Server()
    {
        shutdown_();
        thread_.join();
        close(event_fd_);
    }

    const std::string host;
    const uint16_t port;

private:
    DECLARE_LOGGER("CapnProtoTestServer");


    int event_fd_;
    std::thread thread_;

    void
    run_()
    {
        capnp::EzRpcServer server(kj::heap<ServerImpl>(),
                                  host,
                                  port);
        eventfd_t event = 0;

        auto promise(kj::evalLater([&]
                                   {
                                       auto fd(server.getLowLevelIoProvider().wrapInputFd(event_fd_));
                                       return fd->read(&event,
                                                       sizeof(event),
                                                       sizeof(event)).then([](size_t)
                                                                           {
                                                                               LOG_INFO("got shutdown request");
                                                                           }).attach(kj::mv(fd));
                                   }));

        promise.wait(server.getWaitScope());
    }

    void
    shutdown_()
    {
        if (event_fd_ >= 0)
        {
            LOG_INFO("requesting shutdown");
            eventfd_t event = 1;
            EXPECT_EQ(0,
                      eventfd_write(event_fd_,
                                    event));
        }
    }
};

#endif

}

struct CapnProtoTest
    : public testing::Test
{
    static const std::string&
    host()
    {
        static const std::string h(System::get_env_with_default("CAPNPROTO_TEST_HOST",
                                                                "localhost"s));
        return h;
    }

    static uint16_t
    port()
    {
        static const uint16_t p(System::get_env_with_default("CAPNPROTO_TEST_PORT",
                                                             23232));
        return p;
    }

    // careful, anything > ~ 8k outstanding promises / client is bound to explode
    // the stack due to promise chaining on the client side.
    static size_t
    qdepth()
    {
        static const size_t q = System::get_env_with_default("CAPNPROTO_TEST_QDEPTH",
                                                             64);
        return q;
    }

    static size_t
    iterations()
    {
        static const size_t i = System::get_env_with_default("CAPNPROTO_TEST_ITERATIONS",
                                                             1000ULL);
        return i;
    }

    static size_t
    clients()
    {
        static const size_t c = System::get_env_with_default("CAPNPROTO_TEST_CLIENTS",
                                                             1ULL);
        return c;
    }

    static size_t
    data_size()
    {
        static const size_t s = System::get_env_with_default("CAPNPROTO_TEST_DATA_SIZE",
                                                             4096ULL);
        return s;
    }

#ifdef ENABLE_CAPNP_RPC_TESTS
    static void
    wait_for_server(Server& server,
                    const std::chrono::seconds t = std::chrono::seconds(1))
    {
        using Clock = std::chrono::steady_clock;
        const Clock::time_point timeout = Clock::now() + t;

        while (Clock::now() < timeout)
        {
            try
            {
                capnp::EzRpcClient client(server.host,
                                          server.port);
                auto methods(client.getMain<capnp_test::Methods>());
                methods.nopRequest().send().wait(client.getWaitScope());
                return;
            }
            catch (kj::Exception&)
            {}

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        FAIL() << "failed to connect to server";
        throw std::runtime_error("failed to connect to server");
    }
#endif

    void
    test_serialization_performance(bool zcopy)
    {
        std::vector<uint8_t> buf(data_size() * qdepth());

        size_t serialized_size = 0;

        wall_timer t;

        for (size_t i = 0; i < iterations(); ++i)
        {
            capnp::MallocMessageBuilder mb;
            capnp::Orphanage orphanage(mb.getOrphanage());

            auto broot(mb.initRoot<capnp_test::Methods::WriteParams>());

            capnp::List<capnp_test::Descriptor>::Builder lb(broot.initDescs(qdepth()));
            for (size_t j = 0; j < qdepth(); ++j)
            {
                auto e = lb[j];
                e.setSeqNum(i);

                if (data_size() >= sizeof(i))
                {
                    uint64_t& data = *reinterpret_cast<uint64_t*>(buf.data() + j * data_size());
                    data = i;
                }

                if (data_size() > 0)
                {
                    capnp::Data::Reader
                        reader(static_cast<const kj::byte*>(buf.data() + j * data_size()),
                               data_size());
                    if (zcopy)
                    {
                        capnp::Orphan<capnp::Data>
                            orphan(orphanage.referenceExternalData(std::move(reader)));
                        e.adoptData(std::move(orphan));
                    }
                    else
                    {
                        e.setData(std::move(reader));
                    }
                }
            }

            kj::Array<capnp::word> array(capnp::messageToFlatArray(mb));
            if (serialized_size == 0)
            {
                serialized_size = array.size() * sizeof(capnp::word);
            }

            capnp::FlatArrayMessageReader mr(array);
            auto rroot(mr.getRoot<capnp_test::Methods::WriteParams>());

            size_t count = 0;

            for (const auto& v : rroot.getDescs())
            {
                EXPECT_EQ(i,
                          v.getSeqNum());

                if (data_size() > 0)
                {
                    EXPECT_TRUE(v.hasData());
                    EXPECT_EQ(data_size(),
                              v.getData().size());
                }

                if (data_size() >= sizeof(i))
                {
                    const uint64_t& data = *reinterpret_cast<const uint64_t*>(v.getData().begin());
                    EXPECT_EQ(i,
                              data);
                }

                ++count;
            }

            EXPECT_EQ(qdepth(),
                      count);
        }

        const double elapsed = t.elapsed();
        std::cout << "(de)serializing " << iterations() << " messages of " << qdepth() <<
            " descs, data size " << data_size() << ", serialized message size " <<
            serialized_size << " took " << elapsed << " seconds -> " <<
            (iterations() / elapsed) << " per second" << std::endl;
    }
};

#ifdef ENABLE_CAPNP_RPC_TESTS

TEST_F(CapnProtoTest, DISABLED_server_ctor_dtor)
{
    Server server(host(),
                  port());
    wait_for_server(server);
}

TEST_F(CapnProtoTest, DISABLED_async_nop)
{
    Server server(host(),
                  port());

    wait_for_server(server);

    std::vector<std::future<double>> futs;
    futs.reserve(clients());

    using Params = capnp_test::Methods::NopParams;
    using Results = capnp_test::Methods::NopResults;
    using Request = capnp::Request<Params, Results>;
    using Promise = capnp::RemotePromise<Results>;

    for (size_t c = 0; c < clients(); ++c)
    {
        futs.emplace_back(std::async(std::launch::async,
                                     [&]() -> double
                                     {
                                         capnp::EzRpcClient client(host(),
                                                                   port());
                                         auto methods(client.getMain<capnp_test::Methods>());

                                         kj::WaitScope& wscope = client.getWaitScope();

                                         wall_timer t;

                                         std::vector<Request> requests;
                                         std::vector<Promise> promises;

                                         requests.reserve(iterations());
                                         promises.reserve(iterations());

                                         for (size_t i = 0; i < iterations(); ++i)
                                         {
                                             for (size_t j = 0; j < qdepth(); ++j)
                                             {
                                                 requests.push_back(methods.nopRequest());
                                                 promises.push_back(requests.back().send());
                                             }

                                             for (auto& p : promises)
                                             {
                                                 p.wait(wscope);
                                             }

                                             promises.clear();
                                             requests.clear();
                                         }

                                         return t.elapsed();
                                     }));
    }

    for (auto& f : futs)
    {
        const double elapsed = f.get();
        std::cout << iterations() << " nop iterations with qdepth " << qdepth() <<
            " took " << elapsed << " seconds -> " <<
            ((qdepth() * iterations()) / elapsed) << " calls per second" << std::endl;
    }
}

#endif

TEST_F(CapnProtoTest, serialization_performance)
{
    test_serialization_performance(false);
}

TEST_F(CapnProtoTest, serialization_performance_zcopy)
{
    test_serialization_performance(true);
}

}

PRAGMA_IGNORE_WARNING_END;
