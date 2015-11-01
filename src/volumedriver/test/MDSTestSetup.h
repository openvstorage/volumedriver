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

#ifndef VD_TEST_MDS_TEST_SETUP_H_
#define VD_TEST_MDS_TEST_SETUP_H_

#include "../MDSNodeConfig.h"
#include "../metadata-server/Manager.h"
#include "../metadata-server/ServerConfig.h"

#include <atomic>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include <youtils/Logging.h>

#include <backend/BackendConnectionManager.h>

namespace volumedrivertest
{

class MDSTestSetup
{
public:
    explicit MDSTestSetup(const boost::filesystem::path& home);

    ~MDSTestSetup() = default;

    MDSTestSetup(const MDSTestSetup&) = delete;

    MDSTestSetup&
    operator=(const MDSTestSetup&) = delete;

    uint16_t
    next_port();

    metadata_server::ServerConfig
    next_server_config();

    boost::property_tree::ptree&
    make_manager_config(boost::property_tree::ptree& pt,
                        unsigned nservers = 1,
                        std::chrono::seconds poll_interval = std::chrono::seconds(300));

    boost::property_tree::ptree&
    make_manager_config(boost::property_tree::ptree& pt,
                        const metadata_server::ServerConfigs& scfgs,
                        std::chrono::seconds poll_interval = std::chrono::seconds(300));

    std::unique_ptr<metadata_server::Manager>
    make_manager(backend::BackendConnectionManagerPtr cm,
                 unsigned nservers = 1,
                 std::chrono::seconds poll_interval = std::chrono::seconds(300));

    static uint16_t base_port_;
    static size_t num_threads_;

private:
    DECLARE_LOGGER("MDSTestSetup");

    std::atomic<uint16_t> next_port_;
    const boost::filesystem::path home_;
    std::shared_ptr<metadata_server::Manager> mds_manager_;
};

}

#endif // !VD_TEST_MDS_TEST_SETUP_H_
