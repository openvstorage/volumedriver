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

#ifndef YT_ASIO_SERVICE_MANAGER_H_
#define YT_ASIO_SERVICE_MANAGER_H_

#include "Assert.h"
#include "BooleanEnum.h"
#include "Logging.h"

#include <functional>

#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>

namespace youtils
{

VD_BOOLEAN_ENUM(ImplicitStrand);

class AsioServiceManager
{
public:
    explicit AsioServiceManager(size_t nthreads,
                                bool io_service_per_thread = true);

    ~AsioServiceManager();

    AsioServiceManager(const AsioServiceManager&) = delete;

    AsioServiceManager&
    operator=(const AsioServiceManager&) = delete;

    ImplicitStrand
    implicit_strand() const
    {
        return
            (threads_.size() == 1 or
             io_services_.size() == threads_.size()) ?
            ImplicitStrand::T :
            ImplicitStrand::F;
    }

    template<typename T,
             typename Hash = std::hash<T>>
    boost::asio::io_service&
    get_service(const T& id)
    {
        ASSERT(not io_services_.empty());
        const size_t h = Hash()(id);
        return io_services_.at(h % io_services_.size());
    }

private:
    DECLARE_LOGGER("AsioServiceManager");

    std::vector<boost::asio::io_service> io_services_;
    std::vector<boost::asio::io_service::work> works_;
    std::vector<boost::thread> threads_;

    void
    stop_() noexcept;

    void
    run_(size_t idx);
};

}

#endif // !YT_ASIO_SERVICE_MANAGER_H_
