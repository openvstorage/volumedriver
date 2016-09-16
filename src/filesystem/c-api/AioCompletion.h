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

#ifndef __AIO_COMPLETION_H
#define __AIO_COMPLETION_H

#include "volumedriver.h"
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
    schedule(ovs_completion_t *completion);
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
#endif
