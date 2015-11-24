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

#ifndef __AIO_COMPLETION_H
#define __AIO_COMPLETION_H

#include "common.h"

#include <limits.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

class AioCompletion
{
public:
    static AioCompletion* get_aio_context();

    void
    stop_completion_loop()
    {
        io_service_.stop();
        group_.join_all();
    }

    void
    schedule(ovs_completion_t *completion)
    {
        completion->_calling = true;
        io_service_.post(boost::bind(completion->complete_cb,
                                     completion,
                                     completion->cb_arg));
    }
private:
    AioCompletion()
    : work_(io_service_)
    {
        //cnanakos: scaling?
        for (int i = 0; i < 4; i++)
        {
            group_.create_thread(boost::bind(&boost::asio::io_service::run,
                                             &io_service_));
        }
    };
    AioCompletion(const AioCompletion&) = delete;
    AioCompletion& operator=(const AioCompletion&) = delete;
    static AioCompletion* aio_completion_instance_;

    boost::asio::io_service io_service_;
    boost::asio::io_service::work work_;
    boost::thread_group group_;
};

AioCompletion*
AioCompletion::get_aio_context()
{
    if (not aio_completion_instance_)
    {
        aio_completion_instance_ = new AioCompletion();
    }
    return aio_completion_instance_;
}

#endif
