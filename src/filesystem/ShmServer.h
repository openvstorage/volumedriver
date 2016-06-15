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

#ifndef __SHM_SERVER_H_
#define __SHM_SERVER_H_

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <youtils/UUID.h>
#include <youtils/Assert.h>
#include <youtils/Logging.h>
#include <youtils/System.h>

#include "ShmProtocol.h"

namespace volumedriverfs
{

namespace ipc = boost::interprocess;
namespace yt = youtils;

template<typename Handler>
class ShmServer
{
public:
    ShmServer(std::unique_ptr<Handler> handler)
        : writerequest_msg_(new ShmWriteRequest())
        , writereply_msg_(new ShmWriteReply())
        , readrequest_msg_(new ShmReadRequest())
        , readreply_msg_(new ShmReadReply())
        , handler_(std::move(handler))
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

        const std::string shm_server_env_var("SHM_SERVER_THREAD_POOL_SIZE");
        thread_pool_size_ =
            yt::System::get_env_with_default<int>(shm_server_env_var, 1);

        for (int i = 0; i < thread_pool_size_; i++)
        {
            read_group_.create_thread(boost::bind(&ShmServer::handle_reads,
                                                  this));
            write_group_.create_thread(boost::bind(&ShmServer::handle_writes,
                                                  this));
        }
    }

    ~ShmServer()
    {
        for (int i = 0; i < thread_pool_size_; i++)
        {
            writerequest_mq_->send(getStopRequest<ShmWriteRequest>(),
                                   writerequest_size,
                                   0);
        }
        {
            unsigned int priority;
            ipc::message_queue::size_type received_size;
            ShmWriteReply write_reply_;

            while (writereply_mq_->try_receive(&write_reply_,
                                               writereply_size,
                                               received_size,
                                               priority))
            {
                if (not write_reply_.stop)
                {
                    LOG_INFO("writereply queue is not empty, client error?");
                    break;
                }
            }
        }

        for (int i = 0; i < thread_pool_size_; i++)
        {
            readrequest_mq_->send(getStopRequest<ShmReadRequest>(),
                                  readrequest_size,
                                  0);
        }
        {
            unsigned int priority;
            ipc::message_queue::size_type received_size;
            ShmReadReply read_reply_;

            while (readreply_mq_->try_receive(&read_reply_,
                                              readreply_size,
                                              received_size,
                                              priority))
            {
                if (not read_reply_.stop)
                {
                    LOG_INFO("readreply queue is not empty, client error?");
                    break;
                }
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
    DECLARE_LOGGER("ShmServer");

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
            writerequest_mq_->receive(writerequest_msg_.get(),
                                      writerequest_size,
                                      received_size,
                                      priority);
            VERIFY(received_size == writerequest_size);
            writereply_msg_->opaque = writerequest_msg_->opaque;
            if (writerequest_msg_->stop)
            {
                break;
            }
            else if (writerequest_msg_->size_in_bytes == 0)
            {
                writereply_msg_->failed = handler_->flush() ? false : true;
                writereply_msg_->size_in_bytes = 0;
                writereply_mq_->send(writereply_msg_.get(),
                                     writereply_size,
                                     0);
            }
            else
            {
                handler_->write(writerequest_msg_.get(),
                                writereply_msg_.get());
                writereply_mq_->send(writereply_msg_.get(),
                                     writereply_size,
                                     0);
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
            readrequest_mq_->receive(readrequest_msg_.get(),
                                     readrequest_size,
                                     received_size,
                                     priority);
            VERIFY(received_size == readrequest_size);
            if (readrequest_msg_->stop)
            {
                break;
            }
            {
                readreply_msg_->opaque = readrequest_msg_->opaque;
                handler_->read(readrequest_msg_.get(),
                               readreply_msg_.get());
                readreply_mq_->send(readreply_msg_.get(),
                                    readreply_size,
                                    0);
            }
        }
    }

    boost::thread_group read_group_;
    boost::thread_group write_group_;
    int thread_pool_size_;

    youtils::UUID writerequest_mq_uuid_;
    std::unique_ptr<ipc::message_queue> writerequest_mq_;

    youtils::UUID writereply_mq_uuid_;
    std::unique_ptr<ipc::message_queue> writereply_mq_;

    youtils::UUID readrequest_mq_uuid_;
    std::unique_ptr<ipc::message_queue> readrequest_mq_;

    youtils::UUID readreply_mq_uuid_;
    std::unique_ptr<ipc::message_queue> readreply_mq_;

    std::unique_ptr<ShmWriteRequest> writerequest_msg_;
    std::unique_ptr<ShmWriteReply> writereply_msg_;
    std::unique_ptr<ShmReadRequest> readrequest_msg_;
    std::unique_ptr<ShmReadReply> readreply_msg_;
    std::unique_ptr<Handler> handler_;
};

}
#endif
