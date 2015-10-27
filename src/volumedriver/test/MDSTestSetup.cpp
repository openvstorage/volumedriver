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
