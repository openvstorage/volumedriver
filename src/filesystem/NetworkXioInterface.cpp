// Copyright 2016 iNuron NV
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
NetworkXioInterface::run()
{
    xio_server_.run();
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
