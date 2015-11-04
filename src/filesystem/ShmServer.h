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

#ifndef __SHM_SERVER_H_
#define __SHM_SERVER_H

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <youtils/UUID.h>
#include <youtils/Assert.h>
#include <youtils/Logging.h>

#include "ShmProtocol.h"

#define NUM_THREADS 1

namespace volumedriverfs
{
namespace ipc = boost::interprocess;

template<typename Handler>
class ShmServer
{
public:
    ShmServer(std::unique_ptr<Handler>&& handler)
    : handler_(std::move(handler))
    {
        VERIFY(not writerequest_mq_uuid_.isNull());

        writerequest_mq_.reset(new ipc::message_queue(ipc::create_only,
                                                      writerequest_mq_uuid_.str().c_str(),
                                                      max_write_queue_size,
                                                      writerequest_size));

        writereply_mq_.reset(new ipc::message_queue(ipc::create_only,
                                                     writereply_mq_uuid_.str().c_str(),
                                                     max_write_queue_size,
                                                     writereply_size));

        readrequest_mq_.reset(new ipc::message_queue(ipc::create_only,
                                                     readrequest_mq_uuid_.str().c_str(),
                                                     max_read_queue_size,
                                                     readrequest_size));

        readreply_mq_.reset(new ipc::message_queue(ipc::create_only,
                                                   readreply_mq_uuid_.str().c_str(),
                                                   max_reply_queue_size,
                                                   readreply_size));

        for (int i = 0; i < NUM_THREADS; i++)
        {
            read_group_.create_thread(boost::bind(&ShmServer::handle_reads,
                                                  this));
            write_group_.create_thread(boost::bind(&ShmServer::handle_writes,
                                                  this));
        }
    }

    ~ShmServer()
    {
        for (int i = 0; i < NUM_THREADS; i++)
        {
            writerequest_mq_->send(getStopRequest<ShmWriteRequest>(),
                                   writerequest_size,
                                   0);
        }
        {
            unsigned int priority;
            ipc::message_queue::size_type received_size;

            if (writereply_mq_->try_receive(getStopRequest<ShmReadReply>(),
                                            writereply_size,
                                            received_size,
                                            priority))
            {
                LOG_INFO("writereply queue is not empty, client error?")
            }
        }

        for (int i = 0; i < NUM_THREADS; i++)
        {
            readrequest_mq_->send(getStopRequest<ShmReadRequest>(),
                                  readrequest_size,
                                  0);
        }
        {
            unsigned int priority;
            ipc::message_queue::size_type received_size;

            if (readreply_mq_->try_receive(getStopRequest<ShmReadReply>(),
                                           readreply_size,
                                           received_size,
                                           priority))
            {
                LOG_INFO("readreply queue, client error?")
            }
        }

        write_group_.join_all();
        read_group_.join_all();
        writerequest_mq_.reset();
        writereply_mq_.reset();
        readrequest_mq_.reset();
        readreply_mq_.reset();

        if (not ipc::message_queue::remove(writerequest_mq_uuid_.str().c_str()))
        {
            LOG_INFO("Cannot remove writerequest queue, client still active?");
        }

        if (not ipc::message_queue::remove(writereply_mq_uuid_.str().c_str()))
        {
            LOG_INFO("Cannot remove writereply queue, client still active?");
        }

        if (not ipc::message_queue::remove(readrequest_mq_uuid_.str().c_str()))
        {
            LOG_INFO("Cannot remove readrequest queue, client still active?");
        }

        if (not ipc::message_queue::remove(readreply_mq_uuid_.str().c_str()))
        {
            LOG_INFO("Cannot remove readreply queue, client still active?");
        }
    }

    DECLARE_LOGGER("ShmServer");

    const youtils::UUID&
    writerequest_uuid()
    {
        return writerequest_mq_uuid_;
    }

    const youtils::UUID&
    writereply_uuid()
    {
        return writereply_mq_uuid_;
    }

    const youtils::UUID&
    readrequest_uuid()
    {
        return readrequest_mq_uuid_;
    }

    const youtils::UUID&
    readreply_uuid()
    {
        return readreply_mq_uuid_;
    }

    uint64_t
    volume_size_in_bytes()
    {
        return handler_->volume_size_in_bytes();
    }

private:
    template<typename T>
    static T*
    getStopRequest()
    {
        static std::unique_ptr<T> val(new T);
        val->stop = true;
        return val.get();
    }

    void
    handle_writes()
    {
        unsigned int priority;
        ipc::message_queue::size_type received_size;
        while (true)
        {
            ShmWriteRequest *writerequest_ = new ShmWriteRequest();
            ShmWriteReply *writereply_ = new ShmWriteReply();
            writerequest_mq_->receive(writerequest_,
                                      writerequest_size,
                                      received_size,
                                      priority);
            VERIFY(received_size == writerequest_size);
            writereply_->opaque = writerequest_->opaque;

            if (writerequest_->stop)
            {
                delete writereply_;
                delete writerequest_;
                break;
            }
            else if (writerequest_->size_in_bytes == 0)
            {
                handler_->flush();
                writereply_->size_in_bytes = 0;
                writereply_mq_->send(writereply_,
                                     writereply_size,
                                     0);
                delete writereply_;
                delete writerequest_;
            }
            else
            {
                handler_->write(writerequest_,
                                writereply_);
                writereply_mq_->send(writereply_,
                                     writereply_size,
                                     0);
                delete writerequest_;
                delete writereply_;
            }
        }
    }

    void
    handle_reads()
    {
        unsigned int priority;
        ipc::message_queue::size_type received_size;
        while (true)
        {
            ShmReadRequest *readrequest_ = new ShmReadRequest();
            readrequest_mq_->receive(readrequest_,
                                     readrequest_size,
                                     received_size,
                                     priority);

            if (readrequest_->stop)
            {
                delete readrequest_;
                break;
            }
            {
                ShmReadReply *readreply_ = new ShmReadReply();
                readreply_->opaque = readrequest_->opaque;
                handler_->read(readrequest_,
                               readreply_);
                VERIFY(readreply_->size_in_bytes == readrequest_->size_in_bytes);
                readreply_mq_->send(readreply_,
                                    readreply_size,
                                    0);
                delete readrequest_;
                delete readreply_;
            }
        }
    }

    boost::thread_group read_group_;
    boost::thread_group write_group_;

    youtils::UUID writerequest_mq_uuid_;
    std::unique_ptr<ipc::message_queue> writerequest_mq_;

    youtils::UUID writereply_mq_uuid_;
    std::unique_ptr<ipc::message_queue> writereply_mq_;

    youtils::UUID readrequest_mq_uuid_;
    std::unique_ptr<ipc::message_queue> readrequest_mq_;

    youtils::UUID readreply_mq_uuid_;
    std::unique_ptr<ipc::message_queue> readreply_mq_;

    std::unique_ptr<Handler> handler_;
};

}
#endif
