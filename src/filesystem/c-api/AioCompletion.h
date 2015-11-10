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
        _io_service.stop();
        _group.join_all();
    }

    void
    schedule(ovs_completion_t *completion)
    {
        completion->_calling = true;
        _io_service.post(boost::bind(completion->complete_cb,
                                     completion,
                                     completion->cb_arg));
    }
private:
    AioCompletion()
    : _work(_io_service)
    {
        //cnanakos: scaling?
        for (int i = 0; i < 4; i++)
        {
            _group.create_thread(boost::bind(&boost::asio::io_service::run,
                                             &_io_service));
        }
    };
    AioCompletion(const AioCompletion&) = delete;
    AioCompletion& operator=(const AioCompletion&) = delete;
    static AioCompletion* _aio_completion_instance;

    boost::asio::io_service _io_service;
    boost::asio::io_service::work _work;
    boost::thread_group _group;
};

AioCompletion*
AioCompletion::get_aio_context()
{
    if (not _aio_completion_instance)
    {
        _aio_completion_instance = new AioCompletion();
    }
    return _aio_completion_instance;
}

#endif
