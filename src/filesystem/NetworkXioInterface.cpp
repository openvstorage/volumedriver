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

#include <boost/exception_ptr.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <filesystem/ObjectRouter.h>
#include <filesystem/Registry.h>

#include <libxio.h>

#include "NetworkXioInterface.h"

namespace volumedriverfs
{

namespace bpt = boost::property_tree;
namespace yt = youtils;
namespace ip = initialized_params;

void
NetworkXioInterface::run(std::promise<void> promise)
{
    xio_server_.run(std::move(promise));
}

void
NetworkXioInterface::shutdown()
{
    xio_server_.shutdown();
}

const char*
NetworkXioInterface::componentName() const
{
    return ip::network_interface_component_name;
}

void
NetworkXioInterface::update(const bpt::ptree& pt,
                            yt::UpdateReport& rep)
{
#define U(var)              \
    (var).update(pt, rep)
    U(network_uri);
#undef U
}

void
NetworkXioInterface::persist(bpt::ptree& pt,
                             const ReportDefault rep) const
{
#define P(var)              \
    (var).persist(pt, rep)
    P(network_uri);
#undef P
}

bool
NetworkXioInterface::checkConfig(const bpt::ptree& /*pt*/,
                                 yt::ConfigurationReport& /*crep*/) const
{
    bool res = true;
    return res;
}

} //namespace volumedriverfs
