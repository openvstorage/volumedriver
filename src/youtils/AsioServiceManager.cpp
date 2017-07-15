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

#include "AsioServiceManager.h"
#include "Catchers.h"

#include <boost/lexical_cast.hpp>

namespace youtils
{

namespace ba = boost::asio;

using namespace std::literals::string_literals;

AsioServiceManager::AsioServiceManager(size_t nthreads,
                                       bool io_service_per_thread)
    : io_services_(io_service_per_thread ? nthreads : 1)
{
    if (nthreads == 0)
    {
        LOG_ERROR("number of I/O service threads must be > 0");
        throw std::logic_error("number of I/O service threads must be > 0");
    }

    works_.reserve(io_services_.size());
    for (auto& s : io_services_)
    {
        works_.emplace_back(s);
    }

    threads_.reserve(nthreads);

    try
    {
        for (size_t i = 0; i < nthreads; ++i)
        {
            threads_.emplace_back(boost::bind(&AsioServiceManager::run_,
                                              this,
                                              i));
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Failed to run I/O threads: " << EWHAT);
            stop_();
            throw;
        });
}

AsioServiceManager::~AsioServiceManager()
{
    stop_();
}

void
AsioServiceManager::stop_() noexcept
{
    works_.clear();

    for (auto& s : io_services_)
    {
        try
        {
            s.stop();
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to stop I/O service: " << EWHAT);
                ASSERT(false == "we shouldn't end up here on I/O service stop");
            });
    }

    for (auto& t : threads_)
    {
        try
        {
            t.join();
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to join I/O thread: " << EWHAT);
                ASSERT(false == "we shouldn't end up here on I/O thread join");
            });
    }
}

void
AsioServiceManager::run_(size_t idx)
{
    const std::string name("asio_svc_"s + boost::lexical_cast<std::string>(idx));
    pthread_setname_np(pthread_self(),
                       name.c_str());

    try
    {
        io_services_.at(idx % io_services_.size()).run();
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("I/O thread " << idx << " caught exception: " << EWHAT << ". Exiting");
            ASSERT(false == "we don't want to end up here, fix the handler(s)!?");
        });
}

}
