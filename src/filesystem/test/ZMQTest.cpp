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

#include "FileSystemTestBase.h"

#include <condition_variable>
#include <future>
#include <mutex>

#include <cppzmq/zmq.hpp>

#include <boost/scope_exit.hpp>

#include <youtils/Catchers.h>
#include <youtils/System.h>
#include <youtils/TestBase.h>
#include <youtils/wall_timer.h>
#include <youtils/System.h>

#include "../MessageUtils.h"
#include "../Protocol.h"
#include "../ZUtils.h"

namespace
{

DECLARE_LOGGER("ZMQTest");

typedef std::mutex lock_type;
typedef std::condition_variable cond_type;

typedef std::function<void()> init_done_fun;

std::thread
zmq_thread(std::function<void(init_done_fun&& post_init)>&& fun)
{
    lock_type lock;
    cond_type cond;
    std::unique_lock<lock_type> u(lock);
    bool up = false;

    std::thread t([&]
                  {
                      auto init_done_fun([&]
                                         {
                                             std::lock_guard<lock_type> g(lock);
                                             up = true;
                                             cond.notify_one();
                                         });

                      try
                      {
                          fun(init_done_fun);
                      }
                      catch (zmq::error_t& e)
                      {
                          if (e.num() == ETERM)
                          {
                              LOG_INFO("stop requested");
                          }
                          else
                          {
                              LOG_ERROR("Caught ZMQ error " << e.what());
                          }
                      }
                      CATCH_STD_ALL_LOG_IGNORE("Caught unexpected exception");
                  });

    cond.wait(u, [&up] { return up == true; });

    return t;
}

}

namespace volumedriverfs
{
template<>
struct ZTraits<uint32_t>
{
    static zmq::message_t
    serialize(const uint32_t& u)
    {
        zmq::message_t msg(sizeof(u));
        memcpy(msg.data(), &u, sizeof(u));
        return msg;
    }

    static void
    deserialize(const zmq::message_t& msg,
                uint32_t& u)
    {
        ASSERT(msg.size() == sizeof(u));
        u = *static_cast<const uint32_t*>(msg.data());
    }
};

}

namespace volumedriverfstest
{

namespace vd = volumedriver;
namespace vfs = volumedriverfs;
namespace yt = youtils;

class ZMQTest
    : public youtilstest::TestBase
{};

TEST_F(ZMQTest, DISABLED_inproc)
{
    const unsigned count = yt::System::get_env_with_default("ZMQTEST_MESSAGE_COUNT",
                                                            10000U);
    const unsigned size = yt::System::get_env_with_default("ZMQTEST_MESSAGE_SIZE",
                                                            4096U);
    const unsigned nclients = yt::System::get_env_with_default("ZMQTEST_CLIENT_THREADS",
                                                               1);

    ASSERT_LT(0U, nclients);

    zmq::context_t ztx(0); // ::sysconf(_SC_NPROCESSORS_ONLN));

    const std::string addr(std::string("inproc://") + yt::UUID().str());

    auto rfun([&](init_done_fun&& init_done)
              {
                  zmq::socket_t router(ztx, ZMQ_ROUTER);
                  vfs::ZUtils::socket_no_linger(router);
                  router.bind(addr.c_str());

                  init_done();

                  while (true)
                  {
                      zmq::pollitem_t item;
                      item.socket = router;
                      item.events = ZMQ_POLLIN;

                      auto ret = zmq::poll(&item, 1, -1);
                      EXPECT_LT(0, ret);
                      if (ret > 0)
                      {
                          LOG_TRACE(ret << " events signalled");
                          ASSERT_EQ(ZMQ_POLLIN, item.revents);

                          zmq::message_t sender_id;
                          router.recv(&sender_id);
                          ZEXPECT_MORE(router, "delimiter");

                          zmq::message_t delim;
                          router.recv(&delim);
                          ASSERT_EQ(0U, delim.size());

                          vfsprotocol::RequestType req_type;
                          vfs::ZUtils::deserialize_from_socket(router, req_type);

                          vfsprotocol::Tag tag;
                          vfs::ZUtils::deserialize_from_socket(router, tag);

                          vfsprotocol::WriteRequest req;
                          vfs::ZUtils::deserialize_from_socket(router, req);
                          ASSERT_TRUE(req.IsInitialized());

                          zmq::message_t wmsg;
                          router.recv(&wmsg);

                          ASSERT_EQ(req.size(), wmsg.size());
                          ZEXPECT_NOTHING_MORE(router);

                          router.send(sender_id, ZMQ_SNDMORE);
                          router.send(delim, ZMQ_SNDMORE);

                          vfsprotocol::ResponseType rsp = vfsprotocol::ResponseType::Ok;

                          vfs::ZUtils::serialize_to_socket(router,
                                                           rsp,
                                                           vfs::MoreMessageParts::T);

                          vfs::ZUtils::serialize_to_socket(router,
                                                           tag,
                                                           vfs::MoreMessageParts::F);
                      }
                      else
                      {
                          LOG_WARN("poll unexpectedly returned " << ret);
                      }
                  }
              });

    std::thread rthread(zmq_thread(rfun));

    BOOST_SCOPE_EXIT((&ztx)(&rthread))
    {
        ztx.close();
        rthread.join();
    }
    BOOST_SCOPE_EXIT_END;

    auto client([&]()
                {
                    zmq::socket_t zock(ztx, ZMQ_REQ);
                    vfs::ZUtils::socket_no_linger(zock);
                    zock.connect(addr.c_str());

                    vfs::ObjectId id("foo");

                    const std::vector<char> wbuf('x', size);

                    for (unsigned i = 0; i < count / nclients; ++i)
                    {
                        vfs::ZUtils::serialize_to_socket(zock,
                                                         vfsprotocol::RequestType::Write,
                                                         vfs::MoreMessageParts::T);

                        const vfsprotocol::Tag tag(i);

                        vfs::ZUtils::serialize_to_socket(zock,
                                                         tag,
                                                         vfs::MoreMessageParts::T);

                        const vfs::Object obj(vfs::ObjectType::Volume,
                                              id);
                        vfs::ZUtils::serialize_to_socket(zock,
                                                         vfsprotocol::MessageUtils::create_write_request(obj,
                                                                                                         wbuf.size(),
                                                                                                         0),
                                                         vfs::MoreMessageParts::T);

                        zmq::message_t wmsg(wbuf.size());
                        memcpy(wmsg.data(), wbuf.data(), wbuf.size());
                        zock.send(wmsg, 0);

                        vfsprotocol::ResponseType rsp;
                        vfs::ZUtils::deserialize_from_socket(zock, rsp);
                        ASSERT_TRUE(vfsprotocol::ResponseType::Ok == rsp);

                        vfsprotocol::Tag rsp_tag;
                        vfs::ZUtils::deserialize_from_socket(zock, rsp_tag);
                        ASSERT_EQ(tag, rsp_tag);
                    }
                });

    std::vector<std::thread> clients;
    clients.reserve(nclients);

    yt::wall_timer w;

    for (unsigned i = 0; i < nclients; ++i)
    {
        clients.push_back(std::thread(client));
    }

    for (auto& c : clients)
    {
        c.join();
    }

    const double elapsed = w.elapsed();

    std::cout << "Handling " << count << " messages of size " << size <<
        " (excluding header!) from " << nclients << " client threads took " <<
        elapsed << " seconds => " << count / elapsed << " IOPS" << std::endl;
}

TEST_F(ZMQTest, no_echoes_from_the_past)
{
    const uint32_t cpus = ::sysconf(_SC_NPROCESSORS_ONLN);
    const uint16_t port = FileSystemTestSetup::port_base();

    zmq::context_t ztx(cpus);

    zmq::socket_t router(ztx, ZMQ_ROUTER);
    vfs::ZUtils::socket_no_linger(router);

    const std::string listen_addr("tcp://127.0.0.1:" +
                                  boost::lexical_cast<std::string>(port));
    router.bind(listen_addr.c_str());

    std::unique_ptr<zmq::socket_t> zock(new zmq::socket_t(ztx, ZMQ_REQ));
    // vfs::ZUtils::socket_no_linger(*zock);

    const std::string addr("tcp://127.0.0.1:" +
                           boost::lexical_cast<std::string>(port));
    zock->connect(addr.c_str());

    uint32_t count = 0;

    auto fun([&]
             {
                 zmq::message_t sender_id;
                 router.recv(&sender_id);

                 zmq::message_t delim;
                 router.recv(&delim);

                 ASSERT_EQ(0U, delim.size());

                 uint32_t n = 0;
                 vfs::ZUtils::deserialize_from_socket(router, n);

                 ASSERT_EQ(count, n);

                 count += 1;

                 router.send(sender_id, ZMQ_SNDMORE);
                 router.send(delim, ZMQ_SNDMORE);
                 vfs::ZUtils::serialize_to_socket(router, count, vfs::MoreMessageParts::F);
             });


    vfs::ZUtils::serialize_to_socket(*zock, count, vfs::MoreMessageParts::F);

    // Make sure the message was sent, otherwise fun() might get stuck if the
    // message wasn't sent before zock is torn down.
    {
        zmq::pollitem_t item;
        item.socket = router;
        item.events = ZMQ_POLLIN;

        int ret = zmq::poll(&item, 1, -1);
        EXPECT_LT(0, ret);
        ASSERT_EQ(ZMQ_POLLIN, item.revents);
    }

    zock.reset(new zmq::socket_t(ztx, ZMQ_REQ));
    // vfs::ZUtils::socket_no_linger(*zock);
    zock->connect(addr.c_str());

    fun();

    ASSERT_EQ(1U, count);
    vfs::ZUtils::serialize_to_socket(*zock, count, vfs::MoreMessageParts::F);

    fun();

    uint32_t n = 0;
    vfs::ZUtils::deserialize_from_socket(*zock, n);
    EXPECT_EQ(2U, n);
}

namespace
{

struct Writer
{
    Writer(zmq::context_t& ctx,
           const std::string& router_id,
           unsigned wsz,
           unsigned cnt)
        : ztx(ctx)
        , wsize(wsz)
        , count(cnt)
    {
        thread = zmq_thread([&](init_done_fun&& init_done)
                            {
                                zmq::socket_t zock(ztx, ZMQ_REQ);
                                vfs::ZUtils::socket_no_linger(zock);
                                zock.connect(router_id.c_str());

                                init_done();
                                do_write(zock);
                            });
    }

    ~Writer() = default;

    Writer(const Writer&) = delete;

    Writer&
    operator=(const Writer&) = delete;

    Writer(Writer&&) = default;

    const yt::UUID id;
    zmq::context_t& ztx;
    const unsigned wsize;
    const unsigned count;
    std::thread thread;

    void
    join()
    {
        thread.join();
    }

    void
    do_write(zmq::socket_t& zock)
    {
        const vfs::ObjectId vid(id.str());
        const std::vector<char> wbuf('x', wsize);

        for (unsigned i = 0; i < count; ++i)
        {
            vfs::ZUtils::serialize_to_socket(zock,
                                             vfsprotocol::RequestType::Write,
                                             vfs::MoreMessageParts::T);

            const vfsprotocol::Tag tag(i);

            vfs::ZUtils::serialize_to_socket(zock,
                                             tag,
                                             vfs::MoreMessageParts::T);

            const vfs::Object obj(vfs::ObjectType::Volume,
                                  vid);

            vfs::ZUtils::serialize_to_socket(zock,
                                             vfsprotocol::MessageUtils::create_write_request(obj,
                                                                                             wbuf.size(),
                                                                                             0),
                                             vfs::MoreMessageParts::T);

            zmq::message_t wmsg(wbuf.size());
            memcpy(wmsg.data(), wbuf.data(), wbuf.size());
            zock.send(wmsg, 0);

            vfsprotocol::ResponseType rsp;
            vfs::ZUtils::deserialize_from_socket(zock, rsp);
            ASSERT_TRUE(vfsprotocol::ResponseType::Ok == rsp);

            vfsprotocol::Tag rsp_tag;
            vfs::ZUtils::deserialize_from_socket(zock, rsp_tag);
            ASSERT_EQ(tag, rsp_tag);
        }
    }
};

struct Worker
{
    explicit Worker(zmq::context_t& ctx)
        : ztx(ctx)
    {
        thread = zmq_thread([&](init_done_fun&& init_done)
                            {
                                zmq::socket_t zock(ztx, ZMQ_DEALER);
                                vfs::ZUtils::socket_no_linger(zock);
                                zock.bind(address().c_str());

                                init_done();
                                do_work(zock);
                            });
    }

    ~Worker() = default;

    Worker(const Worker&) = delete;

    Worker&
    operator=(const Worker&) = delete;

    Worker(Worker&&) = default;

    std::string
    address() const
    {
        return "inproc://" + id.str();
    }

    void
    join()
    {
        thread.join();
    }

    void
    do_work(zmq::socket_t& zock)
    {
        while (true)
        {
            zmq::message_t sender_id;
            zock.recv(&sender_id);
            ZEXPECT_MORE(zock, "delimiter");

            zmq::message_t delim;
            zock.recv(&delim);
            ASSERT_EQ(0U, delim.size());

            vfsprotocol::RequestType req_type;
            vfs::ZUtils::deserialize_from_socket(zock, req_type);

            vfsprotocol::Tag tag;
            vfs::ZUtils::deserialize_from_socket(zock, tag);

            vfsprotocol::WriteRequest req;
            vfs::ZUtils::deserialize_from_socket(zock, req);
            ASSERT_TRUE(req.IsInitialized());

            zmq::message_t wmsg;
            zock.recv(&wmsg);

            ASSERT_EQ(req.size(), wmsg.size());
            ZEXPECT_NOTHING_MORE(zock);

            zock.send(sender_id, ZMQ_SNDMORE);
            zock.send(delim, ZMQ_SNDMORE);

            vfsprotocol::ResponseType rsp = vfsprotocol::ResponseType::Ok;

            vfs::ZUtils::serialize_to_socket(zock,
                                             rsp,
                                             vfs::MoreMessageParts::T);

            vfs::ZUtils::serialize_to_socket(zock,
                                             tag,
                                             vfs::MoreMessageParts::F);
        }
    }

    const yt::UUID id;
    zmq::context_t& ztx;
    std::thread thread;
};

struct Dispatcher
{
    Dispatcher(zmq::context_t& ctx,
               const std::vector<Worker>& workers)
        : ztx(ctx)
    {
        thread = zmq_thread([&](init_done_fun&& init_done)
                            {
                                zmq::socket_t router(ztx, ZMQ_ROUTER);
                                vfs::ZUtils::socket_no_linger(router);
                                router.bind(address().c_str());

                                zmq::socket_t dealer(ztx, ZMQ_DEALER);
                                vfs::ZUtils::socket_no_linger(dealer);

                                for (const auto& w : workers)
                                {
                                    dealer.connect(w.address().c_str());
                                }

                                init_done();

                                zmq::proxy(router, dealer, nullptr);
                            });
    }

    ~Dispatcher() = default;

    Dispatcher(const Dispatcher&) = delete;

    Dispatcher&
    operator=(const Dispatcher&) = delete;

    Dispatcher(Dispatcher&&) = default;

    std::string
    address() const
    {
        return "inproc://" + id.str();
    }

    void
    join()
    {
        thread.join();
    }

    const yt::UUID id;
    zmq::context_t& ztx;
    std::thread thread;
};

}

TEST_F(ZMQTest, DISABLED_inproc_shared_queue)
{
    const unsigned count = yt::System::get_env_with_default("ZMQTEST_MESSAGE_COUNT",
                                                            10000U);
    const unsigned size = yt::System::get_env_with_default("ZMQTEST_MESSAGE_SIZE",
                                                            4096U);
    const unsigned nclients = yt::System::get_env_with_default("ZMQTEST_CLIENT_THREADS",
                                                               1);
    ASSERT_LT(0U, nclients);

    const unsigned nworkers = yt::System::get_env_with_default("ZMQTEST_WORKER_THREADS",
                                                               1);
    ASSERT_LT(0U, nworkers);

    zmq::context_t ztx(0); // ::sysconf(_SC_NPROCESSORS_ONLN));

    std::vector<Worker> workers;
    workers.reserve(nworkers);

    BOOST_SCOPE_EXIT((&ztx)(&workers))
    {
        ztx.close();

        for (auto& w : workers)
        {
            w.join();
        }
    }
    BOOST_SCOPE_EXIT_END;

    for (unsigned i = 0; i < nworkers; ++i)
    {
        workers.emplace_back(Worker(ztx));
    }

    Dispatcher dispatcher(ztx, workers);

    BOOST_SCOPE_EXIT((&ztx)(&dispatcher))
    {
        ztx.close();
        dispatcher.join();
    }
    BOOST_SCOPE_EXIT_END;

    std::vector<Writer> writers;
    writers.reserve(nclients);

    yt::wall_timer w;

    {
        BOOST_SCOPE_EXIT((&writers))
        {
            for (auto& wr : writers)
            {
                wr.join();
            }
        }
        BOOST_SCOPE_EXIT_END;

        for (unsigned i = 0; i < nclients; ++i)
        {
            Writer w(ztx,
                     dispatcher.address(),
                     size,
                     count / nclients);

            writers.emplace_back(std::move(w));
        }
    }

    const double elapsed = w.elapsed();

    std::cout << "Handling " << count << " messages of size " << size <<
        " (excluding header!) from " << nclients << " client threads, " << nworkers <<
        " worker threads took " << elapsed << " seconds => " << count / elapsed <<
        " IOPS" << std::endl;
}

// Building an elastic worker pool this way with a DEALER socket does not work as
// showcased by the test result.
TEST_F(ZMQTest, DISABLED_elastic_worker_pool_based_on_dealer)
{
    const unsigned wcount = 8;
    std::vector<boost::thread> workers;
    workers.reserve(wcount);

    const uint32_t timeout_secs = 30;
    yt::wall_timer wt;

    {
        zmq::context_t ztx(0);
        const std::string addr("inproc://" + yt::UUID().str());

        zmq::socket_t dealer(ztx, ZMQ_DEALER);
        const std::string id(yt::UUID().str());
        dealer.setsockopt(ZMQ_IDENTITY, id.c_str(), id.size());

        vfs::ZUtils::socket_no_linger(dealer);
        dealer.bind(addr.c_str());

        for (unsigned i = 0; i < wcount; ++i)
        {
            std::promise<bool> promise;
            std::future<bool> future(promise.get_future());

            auto wfun([&addr, i, &id, &promise, &timeout_secs, &ztx]
                      {
                          try
                          {
                              zmq::socket_t router(ztx, ZMQ_ROUTER);
                              vfs::ZUtils::socket_no_linger(router);
                              router.connect(addr.c_str());

                              promise.set_value(true);

                              while (true)
                              {
                                  zmq::message_t rxid;
                                  router.recv(&rxid);

                                  EXPECT_EQ(id,
                                            std::string(static_cast<const char*>(rxid.data()),
                                                        rxid.size()));

                                  ZEXPECT_MORE(router, "delimiter");

                                  zmq::message_t delim;
                                  router.recv(&delim);

                                  EXPECT_EQ(0U, delim.size());

                                  ZEXPECT_MORE(router, "payload");

                                  zmq::message_t msg;
                                  router.recv(&msg);

                                  ZEXPECT_NOTHING_MORE(router);

                                  ASSERT_EQ(sizeof(unsigned), msg.size());
                                  EXPECT_EQ(i, *static_cast<unsigned*>(msg.data()));

                                  LOG_INFO("worker " << i << ": rcvd " <<
                                           *static_cast<unsigned*>(msg.data()));

                                  boost::this_thread::sleep_for(boost::chrono::seconds(timeout_secs));

                                  router.send(rxid, ZMQ_SNDMORE);
                                  router.send(delim, ZMQ_SNDMORE);
                                  router.send(msg, 0);
                              }
                          }
                          catch (zmq::error_t& e)
                          {
                              if (e.num() == ETERM)
                              {
                                  LOG_INFO( "worker " << i << ": termination requested");
                              }
                              else
                              {
                                  FAIL() << "worker " << i << " ZMQ error: " << e.what() <<
                                      ", num: " << e.num();
                              }
                          }
                          CATCH_STD_ALL_EWHAT({
                                  FAIL() << "worker " << i << " caught exception: " << EWHAT;
                              });
                      });

            workers.emplace_back(boost::thread(wfun));

            ASSERT_TRUE(future.get());

            zmq::message_t delim(0);
            dealer.send(delim, ZMQ_SNDMORE);

            zmq::message_t msg(sizeof(i));
            memcpy(msg.data(), &i, sizeof(i));
            dealer.send(msg, 0);

            LOG_INFO("dealer: sent " << i);
        }

        std::set<unsigned> set;

        for (unsigned i = 0; i < workers.size(); ++i)
        {
            zmq::message_t delim;
            dealer.recv(&delim);
            ZEXPECT_MORE(dealer, "payload");

            zmq::message_t msg;
            dealer.recv(&msg);
            ZEXPECT_NOTHING_MORE(dealer);

            ASSERT_EQ(sizeof(unsigned), msg.size());
            const unsigned n = *static_cast<unsigned*>(msg.data());

            LOG_INFO("dealer: recv #" << i << ", data " << n);

            const auto r(set.insert(n));
            EXPECT_TRUE(r.second);
        }

        ASSERT_EQ(workers.size(), set.size());

        for (unsigned i = 0; i < set.size(); ++i)
        {
            ASSERT_TRUE(set.find(i) != set.end());
        }
    }

    for (auto& w : workers)
    {
        w.join();
    }

    ASSERT_LT(timeout_secs, wt.elapsed());
    ASSERT_GT(timeout_secs * 2, wt.elapsed());
}

TEST_F(ZMQTest, DISABLED_elastic_worker_pool_based_on_router)
{
    const unsigned wcount = 8;
    std::vector<boost::thread> workers;
    workers.reserve(wcount);

    const uint32_t timeout_secs = 10;
    yt::wall_timer wt;

    {
        zmq::context_t ztx(0);
        const std::string addr("inproc://" + yt::UUID().str());

        zmq::socket_t router(ztx, ZMQ_ROUTER);
        const std::string id(yt::UUID().str());
        router.setsockopt(ZMQ_IDENTITY, id.c_str(), id.size());

        vfs::ZUtils::socket_no_linger(router);
        router.bind(addr.c_str());

        for (unsigned i = 0; i < wcount; ++i)
        {
            std::promise<std::string> promise;
            std::future<std::string> future(promise.get_future());

            auto wfun([&addr, i, &id, &promise, &timeout_secs, &ztx]
                      {
                          try
                          {
                              zmq::socket_t zock(ztx, ZMQ_REP);

                              const std::string sockid(yt::UUID().str());
                              zock.setsockopt(ZMQ_IDENTITY,
                                              sockid.c_str(),
                                              sockid.size());

                              vfs::ZUtils::socket_no_linger(zock);
                              zock.connect(addr.c_str());

                              promise.set_value(sockid);

                              while (true)
                              {
                                  zmq::message_t msg;
                                  zock.recv(&msg);

                                  ZEXPECT_NOTHING_MORE(zock);

                                  ASSERT_EQ(sizeof(unsigned), msg.size());
                                  EXPECT_EQ(i, *static_cast<unsigned*>(msg.data()));

                                  LOG_INFO("sock " << i << ": rcvd " <<
                                           *static_cast<unsigned*>(msg.data()));

                                  boost::this_thread::sleep_for(boost::chrono::seconds(timeout_secs));

                                  zock.send(msg, 0);
                              }
                          }
                          catch (zmq::error_t& e)
                          {
                              if (e.num() == ETERM)
                              {
                                  LOG_INFO( "worker " << i << ": termination requested");
                              }
                              else
                              {
                                  FAIL() << "worker " << i << " ZMQ error: " << e.what() <<
                                      ", num: " << e.num();
                              }
                          }
                          CATCH_STD_ALL_EWHAT({
                                  FAIL() << "worker " << i << " caught exception: " << EWHAT;
                              });
                      });

            workers.emplace_back(boost::thread(wfun));

            const std::string worker_id(future.get());
            zmq::message_t wid(worker_id.size());
            memcpy(wid.data(), worker_id.c_str(), worker_id.size());

            router.send(wid, ZMQ_SNDMORE);

            zmq::message_t delim(0);
            router.send(delim, ZMQ_SNDMORE);

            zmq::message_t msg(sizeof(i));
            memcpy(msg.data(), &i, sizeof(i));
            router.send(msg, 0);

            LOG_INFO("router: sent " << i);
        }

        std::set<unsigned> set;

        for (unsigned i = 0; i < workers.size(); ++i)
        {
            zmq::message_t sockid;
            router.recv(&sockid);

            ZEXPECT_MORE(router, "delim");

            zmq::message_t delim;
            router.recv(&delim);
            ZEXPECT_MORE(router, "payload");

            zmq::message_t msg;
            router.recv(&msg);
            ZEXPECT_NOTHING_MORE(router);

            ASSERT_EQ(sizeof(unsigned), msg.size());
            const unsigned n = *static_cast<unsigned*>(msg.data());

            LOG_INFO("router: recv #" << i << ", data " << n);

            const auto r(set.insert(n));
            EXPECT_TRUE(r.second);
        }

        ASSERT_EQ(workers.size(), set.size());

        for (unsigned i = 0; i < set.size(); ++i)
        {
            ASSERT_TRUE(set.find(i) != set.end());
        }
    }

    for (auto& w : workers)
    {
        w.join();
    }

    ASSERT_LT(timeout_secs, wt.elapsed());
    ASSERT_GT(timeout_secs * 2, wt.elapsed());
}

TEST_F(ZMQTest, DISABLED_reset_inproc_rep_socket)
{
    const std::string addr("inproc://" + yt::UUID().str());
    zmq::context_t ztx(0);
    zmq::socket_t req(ztx, ZMQ_REQ);
    vfs::ZUtils::socket_no_linger(req);
    req.bind(addr.c_str());

    auto make_rep([&]
                  {
                      std::unique_ptr<zmq::socket_t> rep(new zmq::socket_t(ztx, ZMQ_REP));
                      vfs::ZUtils::socket_no_linger(*rep);
                      rep->connect(addr.c_str());
                      return rep;
                  });

    std::promise<bool> promise;
    std::future<bool> future(promise.get_future());
    boost::thread thread([&]
                         {
                             auto rep(make_rep());
                             promise.set_value(true);

                             zmq::message_t msg;
                             rep->recv(&msg);

                             rep.reset();

                             EXPECT_NO_THROW(rep = make_rep());
                         });

    EXPECT_TRUE(future.get());

    const std::string ping("Ping!");
    {
        zmq::message_t msg(ping.size());
        memcpy(msg.data(), ping.data(), msg.size());
        req.send(msg, 0);
    }

    thread.join();
}

}
