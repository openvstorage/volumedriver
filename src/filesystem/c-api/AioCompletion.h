// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
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
#include <youtils/System.h>

class AioCompletion
{
public:
    static AioCompletion& get_aio_context();

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
        const std::string aio_env_var("LIBOVSVOLUMEDRIVER_AIO_COMP_POOL_SIZE");
        int pool_size =
            youtils::System::get_env_with_default<int>(aio_env_var, 4);
        for (int i = 0; i < pool_size; i++)
        {
            group_.create_thread(boost::bind(&boost::asio::io_service::run,
                                             &io_service_));
        }
    };

    ~AioCompletion()
    {
        try
        {
            io_service_.stop();
            group_.join_all();
        }
        catch (...) {}
    }

    AioCompletion(const AioCompletion&) = delete;
    AioCompletion& operator=(const AioCompletion&) = delete;

    boost::asio::io_service io_service_;
    boost::asio::io_service::work work_;
    boost::thread_group group_;
};

AioCompletion&
AioCompletion::get_aio_context()
{
    static AioCompletion aio_completion_instance_;
    return aio_completion_instance_;
}

#endif
