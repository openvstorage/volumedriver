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

#include "MDSTestSetup.h"

#include "../MetaDataBackendConfig.h"
#include "../metadata-server/ClientNG.h"
#include "../metadata-server/Parameters.h"
#include "../metadata-server/Manager.h"

#include <boost/property_tree/ptree.hpp>

#include <youtils/System.h>

namespace volumedrivertest
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace mds = metadata_server;
namespace vd = volumedriver;
namespace yt = youtils;

namespace
{

const std::string address("127.0.0.1");

}

uint16_t
MDSTestSetup::base_port_ = youtils::System::get_env_with_default<uint16_t>("MDS_PORT_BASE",
                                                                           18081);

size_t
MDSTestSetup::num_threads_ = yt::System::get_env_with_default<size_t>("MDS_THREADS",
                                                                      1);


MDSTestSetup::MDSTestSetup(const fs::path& home)
    : next_port_(base_port_)
    , home_(home)
{}

uint16_t
MDSTestSetup::next_port()
{
    return ++next_port_;
}

mds::ServerConfig
MDSTestSetup::next_server_config()
{
    const uint16_t port = next_port();
    const fs::path p(home_ / boost::lexical_cast<std::string>(port));
    const fs::path db(p / "db");
    const fs::path scratch(p / "scratch");

    return mds::ServerConfig(vd::MDSNodeConfig(address,
                                               port),
                             db,
                             scratch);
}

bpt::ptree&
MDSTestSetup::make_manager_config(bpt::ptree& pt,
                                  unsigned nservers,
                                  std::chrono::seconds poll_secs)
{
    mds::ServerConfigs cfgs;
    cfgs.reserve(nservers);

    for (unsigned i = 0; i < nservers; ++i)
    {
        cfgs.emplace_back(next_server_config());
    }

    return make_manager_config(pt,
                               cfgs,
                               poll_secs);
}

bpt::ptree&
MDSTestSetup::make_manager_config(bpt::ptree& pt,
                                  const mds::ServerConfigs& cfgs,
                                  std::chrono::seconds poll_secs)
{
    PARAMETER_TYPE(ip::mds_nodes)(cfgs).persist(pt);
    PARAMETER_TYPE(ip::mds_cached_pages)(256).persist(pt);
    PARAMETER_TYPE(ip::mds_poll_secs)(poll_secs.count()).persist(pt);
    PARAMETER_TYPE(ip::mds_threads)(num_threads_).persist(pt);

    return pt;
}

std::unique_ptr<mds::Manager>
MDSTestSetup::make_manager(be::BackendConnectionManagerPtr cm,
                           unsigned nservers,
                           std::chrono::seconds poll_secs)
{
    bpt::ptree pt;
    return std::make_unique<mds::Manager>(make_manager_config(pt,
                                                              nservers,
                                                              poll_secs),
                                          RegisterComponent::F,
                                          cm);
}

}
