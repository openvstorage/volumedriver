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
    make_client(const Cluster::MaybeMilliSeconds& mms = boost::none) const;

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

    boost::filesystem::path
    arakoonServerConfigPath_() const;

    void
    createArakoonConfigFile_(const boost::filesystem::path& cfgfile);

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
