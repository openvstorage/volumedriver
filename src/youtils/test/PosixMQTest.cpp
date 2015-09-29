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

#include "../Assert.h"
#include "../Catchers.h"
#include "../Logging.h"
#include "../System.h"
#include "../TestBase.h"
#include "../UUID.h"
#include "../wall_timer.h"

#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>

#include <boost/asio.hpp>
#include <boost/thread.hpp>

namespace youtilstest
{

namespace ba = boost::asio;
namespace bs = boost::system;
namespace yt = youtils;

using namespace std::literals::string_literals;

namespace
{

DECLARE_LOGGER("PosixMQUtils");

mqd_t
make_queue(const std::string& id,
           bool read,
           size_t msg_size)
{
    mq_attr attr;
    memset(&attr, 0x0, sizeof(attr));
    attr.mq_maxmsg = 1; // for now we don't do queuing anyway
    attr.mq_msgsize = static_cast<long>(msg_size);

    mqd_t res = ::mq_open(id.c_str(),
                          (read ? O_RDONLY : O_WRONLY) bitor O_CREAT bitor O_NONBLOCK bitor O_EXCL,
                          S_IRWXU,
                          &attr);
    if (res < 0)
    {
        LOG_ERROR("Failed to create queue: " << strerror(errno));
        throw std::runtime_error("Failed to create queue");
    }

    return res;
}

class Server
{
    DECLARE_LOGGER("PosixMQServer");

public:
    Server(const size_t msg_size,
           const size_t nthreads)
        : sq_(make_queue(send_queue_id(),
                         false,
                         msg_size))
        , rq_(make_queue(recv_queue_id(),
                         true,
                         msg_size))
        , strand_(io_service_)
        , sq_desc_(io_service_,
                   sq_)
        , rq_desc_(io_service_,
                   rq_)
        , buf_(msg_size)
    {
        VERIFY(sq_ > -1);
        VERIFY(rq_ > -1);

        post_recv_();

        try
        {
            for (size_t i = 0; i < nthreads; ++i)
            {
                threads_.create_thread(boost::bind(&Server::work_,
                                                   this));
            }
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to create worker pool: " << EWHAT);
                stop_();
                throw;
            });
    }

    ~Server()
    {
        try
        {
            stop_();
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to stop PosixMQServer");
    }

    Server(const Server&) = delete;

    Server&
    operator=(const Server&) = delete;

    std::string
    send_queue_id() const
    {
        return "/"s + uuid_.str() + "-sq";
    }

    std::string
    recv_queue_id() const
    {
        return "/"s + uuid_.str() + "-rq";
    }

private:
    const yt::UUID uuid_;
    ba::io_service io_service_;
    ::mqd_t sq_;
    ::mqd_t rq_;
    ba::strand strand_;
    ba::posix::stream_descriptor sq_desc_;
    ba::posix::stream_descriptor rq_desc_;
    std::vector<char> buf_;
    boost::thread_group threads_;

    void
    stop_()
    {
        io_service_.stop();
        threads_.join_all();

        ::mq_close(rq_);
        ::mq_close(sq_);
        ::mq_unlink(recv_queue_id().c_str());
        ::mq_unlink(send_queue_id().c_str());
    }

    void
    work_()
    {
        while (true)
        {
            try
            {
                io_service_.run();
                LOG_INFO("I/O service exited, breaking out of loop");
                return;
            }
            CATCH_STD_ALL_EWHAT({
                    LOG_ERROR("I/O service caught exception: " << EWHAT <<
                              "- breaking out of loop");
                    return;
                });
        }
    }

    void
    post_recv_()
    {
        auto f([this]
               (const bs::error_code& ec,
                std::size_t /* bytes_transferred */)
               {
                   if (ec)
                   {
                       LOG_ERROR("recv completed with error: " << ec.message());
                   }
                   else
                   {
                       unsigned prio = 0;
                       ssize_t res = mq_receive(rq_,
                                                buf_.data(),
                                                buf_.size(),
                                                &prio);

                       if (res != static_cast<ssize_t>(buf_.size()))
                       {
                           if (res < 0)
                           {
                               LOG_ERROR("recv error: " << strerror(errno));
                           }
                           else
                           {
                               LOG_ERROR("unexpected transfer: got " << res <<
                                         ", expected " << buf_.size());
                           }
                       }
                       else
                       {
                           post_send_();
                       }
                   }
               });

        rq_desc_.async_read_some(ba::null_buffers(),
                                 strand_.wrap(f));
    }

    void
    post_send_()
    {
        auto f([this]
               (const bs::error_code& ec,
                std::size_t /* bytes_transferred */)
               {
                   if (ec)
                   {
                       LOG_ERROR("send completed with error: " << ec.message());
                   }
                   else
                   {
                       unsigned prio = 0;
                       ssize_t res = mq_send(sq_,
                                             buf_.data(),
                                             buf_.size(),
                                             prio);

                       if (res != 0)
                       {
                           LOG_ERROR("send error: " << strerror(errno));
                       }
                       else
                       {
                           post_recv_();
                       }
                   }
               });

        sq_desc_.async_write_some(ba::null_buffers(),
                                  strand_.wrap(f));
    }
};

class Client
{
    DECLARE_LOGGER("PosixMQClient");

public:
    Client(const std::string& sq_name,
           const std::string& rq_name,
           uint64_t msg_size)
        : sq_(::mq_open(sq_name.c_str(),
                        O_WRONLY))
        , rq_(::mq_open(rq_name.c_str(),
                        O_RDONLY))
        , sbuf_(msg_size, 'S')
        , rbuf_(msg_size, 'R')
    {}

    ~Client()
    {
        ::mq_close(rq_);
        ::mq_close(sq_);
    }

    Client(const Client&) = delete;

    Client&
    operator=(const Client&) = delete;

    void
    send()
    {
        unsigned prio = 0;
        ssize_t res = mq_send(sq_,
                              sbuf_.data(),
                              sbuf_.size(),
                              prio);

        if (res != 0)
        {
            LOG_ERROR("failed to send: " << strerror(errno));
            throw std::runtime_error("failed to send");
        }
    }

    void
    recv()
    {
        unsigned prio = 0;
        ssize_t res = mq_receive(rq_,
                                 rbuf_.data(),
                                 rbuf_.size(),
                                 &prio);

        if (res < 0)
        {
            LOG_ERROR("failed to recv: " << strerror(errno));
            throw std::runtime_error("failed to recv");
        }
        else if (res != static_cast<ssize_t>(rbuf_.size()))
        {
            LOG_ERROR("recv size " << res << " != expected size " << rbuf_.size());
            throw std::runtime_error("failed to recv");
        }
    }

    ::mqd_t sq_;
    ::mqd_t rq_;

public:
    std::vector<char> sbuf_;
    std::vector<char> rbuf_;
};

}

class PosixMQTest
    : public TestBase
{
    DECLARE_LOGGER("PosixMQTest");

protected:
    PosixMQTest()
        : size_(yt::System::get_env_with_default<uint32_t>("POSIXMQ_MESSAGE_SIZE",
                                                           4096))
        , iterations_(yt::System::get_env_with_default<uint32_t>("POSIXMQ_MESSAGE_COUNT",
                                                                 10000))
        , nthreads_(yt::System::get_env_with_default<uint32_t>("POSIXMQ_SERVER_THREADS",
                                                               1))
    {}

    const uint32_t size_;
    const uint32_t iterations_;
    const uint32_t nthreads_;
};

// breaks mysteriously when running more than 1 server thread.
TEST_F(PosixMQTest, DISABLED_echo)
{
    Server server(size_,
                  nthreads_);

    Client client(server.recv_queue_id(),
                  server.send_queue_id(),
                  size_);

    yt::wall_timer w;

    for (size_t i = 0; i < iterations_; ++i)
    {
        client.send();
        client.recv();
    }

    const double t = w.elapsed();

    std::cout << iterations_ << " iterations with messages of " << size_ <<
        " bytes took " << t << " seconds -> " << iterations_ / t <<
        " IOPS" << std::endl;

    EXPECT_EQ(0, memcmp(client.sbuf_.data(),
                        client.rbuf_.data(),
                        client.sbuf_.size()));
}

}
