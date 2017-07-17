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

#include "AsioServiceManagerComponent.h"

#include <boost/property_tree/ptree.hpp>

#include <youtils/AsioServiceManager.h>

namespace volumedriver
{

namespace bpt = boost::property_tree;
namespace yt = youtils;

AsioServiceManagerComponent::AsioServiceManagerComponent(const bpt::ptree& pt,
                                                         const RegisterComponent registerize)
    : yt::VolumeDriverComponent(registerize,
                                pt)
    , asio_service_manager_threads(pt)
    , asio_service_manager_io_service_per_thread(pt)
    , manager_(std::make_shared<yt::AsioServiceManager>(asio_service_manager_threads.value() ?
                                                        asio_service_manager_threads.value() :
                                                        boost::thread::hardware_concurrency(),
                                                        asio_service_manager_io_service_per_thread.value()))
{}

void
AsioServiceManagerComponent::update(const bpt::ptree& pt,
                                    yt::UpdateReport& report)
{
#define U(x)                                    \
    x.update(pt,                                \
             report)

    U(asio_service_manager_threads);
    U(asio_service_manager_io_service_per_thread);
#undef U
}

void
AsioServiceManagerComponent::persist(bpt::ptree& pt,
                                     const ReportDefault report_default) const
{
#define P(x)                                     \
    x.persist(pt,                                \
              report_default)

    P(asio_service_manager_threads);
    P(asio_service_manager_io_service_per_thread);
#undef P
}

bool
AsioServiceManagerComponent::checkConfig(const bpt::ptree&,
                                         yt::ConfigurationReport&) const
{
    return true;
}

}
