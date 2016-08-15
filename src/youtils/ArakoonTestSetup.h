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

#ifndef ARAKOON_TEST_SETUP_H_
#define ARAKOON_TEST_SETUP_H_

#include "ArakoonInterface.h"
#include "ArakoonNodeConfig.h"
#include <pstreams/pstream.h>
#include "Logging.h"

#include <boost/filesystem.hpp>

namespace arakoon
{

class ArakoonTestSetup
{
public:
    // Move this out...
    static void
    setArakoonBinaryPath(const boost::filesystem::path&);

    static void
    setArakoonBasePort(const uint16_t base_port);

    static void
    setArakoonPlugins(const std::vector<boost::filesystem::path>& plugins);

    ArakoonTestSetup(const boost::filesystem::path& basedir,
                     const uint16_t num_nodes = 1,
                     const std::string& node_name = "arakoon_node",
                     const std::string& cluster_name = "arakoon_cluster",
                     const std::string& log_level = "debug");

    void
    setUpArakoon();

    void
    tearDownArakoon(bool cleanup = false);

    static bool
    useArakoon();

    NodeID
    nodeID(const uint16_t node_index) const;

    ClusterID
    clusterID() const;

    uint16_t
    num_nodes() const;

    std::list<ArakoonNodeConfig>
    node_configs() const;

    const std::string&
    host_name() const
    {
        return host_name_;
    }

    uint16_t
    client_port(const uint16_t node_index) const
    {
        return port_base_ + 2*node_index;
    }

    uint16_t
    message_port(const uint16_t node_index) const
    {
        return client_port(node_index) + 1;
    }

    ArakoonNodeConfig
    node_config(const uint16_t index) const
    {
        return ArakoonNodeConfig(nodeID(index),
                                 host_name(),
                                 client_port(index));
    }

    void
    shootDownNode(const std::string& node,
                  const int signal);

    void
    start_nodes();

    void
    stop_nodes();

    std::unique_ptr<Cluster>
    make_client(const Cluster::MaybeMilliSeconds& = boost::none) const;

    void
    write_config(std::ostream&);

    boost::filesystem::path
    server_config_file() const;

protected:
    DECLARE_LOGGER("ArakoonTestSetup");

    const std::string host_name_;

    const std::string cluster_name_;
    const std::string node_name_;

    pid_t pid_;
    boost::filesystem::path dir_;

    const std::string log_level_;

    const uint16_t num_nodes_;

    static uint16_t port_base_;
    static boost::filesystem::path binary_path_;
    static std::vector<boost::filesystem::path> plugins_;
    static std::string server_cfg_;
    static std::string client_cfg_;

    typedef std::map<const std::string, redi::ipstream*> node_map;
    typedef node_map::iterator node_map_iterator;
    typedef node_map::const_iterator node_map_const_iterator;

    node_map nodes;

    boost::filesystem::path
    node_home_dir_(unsigned id) const;

    void
    waitForArakoon_() const;

    void
    checkArakoonNotRunning(const uint16_t i) const;

    void
    symlink_plugins_() const;
};

}

#endif // ! ARAKOON_TEST_SETUP_H_

// Local Variables: **
// mode: c++ **
// End: **
