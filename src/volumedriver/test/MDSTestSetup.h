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
