// Copyright 2015 Open vStorage NV
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

#include "RocksDataBase.h"
#include "ServerNG.h"
#include "Manager.h"
#include "Table.h"
#include "WeakDataBase.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/thread.hpp>

namespace metadata_server
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace vd = volumedriver;
namespace yt = youtils;

using namespace std::literals::string_literals;

#define LOCK()                                          \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

Manager::Manager(const bpt::ptree& pt,
                 const RegisterComponent registerizle,
                 be::BackendConnectionManagerPtr cm)
    : VolumeDriverComponent(registerizle,
                            pt)
    , mds_poll_secs(pt)
    , mds_threads(pt)
    , mds_timeout_secs(pt)
    , mds_cached_pages(pt)
    , cm_(cm)
{
    LOCK();
    nodes_ = make_nodes_((ip::PARAMETER_TYPE(mds_nodes)(pt)).value());
    log_nodes_("construction");
}

Manager::~Manager()
{
    LOCK();
    nodes_.clear();
}

DataBaseInterfacePtr
Manager::find(const vd::MDSNodeConfig& ncfg) const
{
    LOCK();

    for (const auto& n : nodes_)
    {
        if (ncfg == n.first.node_config)
        {
            return std::make_shared<WeakDataBase>(n.second->database());
        }
    }

    return nullptr;
}

ServerConfigs
Manager::server_configs() const
{
    LOCK();

    ServerConfigs cfgs;
    cfgs.reserve(nodes_.size());

    for (const auto& n : nodes_)
    {
        cfgs.emplace_back(n.first);
    }

    return cfgs;
}

void
Manager::update(const bpt::ptree& pt,
                yt::UpdateReport& rep)
{
    LOCK();

#define U(var)                                  \
    (var).update(pt, rep)

    U(mds_poll_secs);
    U(mds_threads);
    U(mds_timeout_secs);
    U(mds_cached_pages);

    PARAMETER_TYPE(ip::mds_nodes) nodes(pt);
    nodes_ = make_nodes_(nodes.value());
    log_nodes_("config update");

    U(nodes);

#undef U
}

void
Manager::persist(bpt::ptree& pt,
                 const ReportDefault report_default) const
{
#define P(var)                                  \
    (var).persist(pt, report_default)

    P(mds_poll_secs);
    P(mds_threads);
    P(mds_timeout_secs);
    P(mds_cached_pages);

    using MDSNodesType = PARAMETER_TYPE(ip::mds_nodes);
    MDSNodesType::ValueType nodes;

    for (const auto& n : nodes_)
    {
        nodes.push_back(n.first);
    }

    MDSNodesType(nodes).persist(pt, report_default);

#undef P
}

bool
Manager::checkConfig(const bpt::ptree& pt,
                     yt::ConfigurationReport& crep) const
{
    using MDSNodesType = PARAMETER_TYPE(ip::mds_nodes);
    const MDSNodesType nodes(pt);

    auto maybe_str(check_configs_(nodes.value()));
    if (maybe_str)
    {
        LOG_INFO("config problem: " << *maybe_str);
        crep.push_back(yt::ConfigurationProblem(nodes.name(),
                                                nodes.section_name(),
                                                *maybe_str));
        return false;
    }
    else
    {
        return true;
    }
}

boost::optional<std::string>
Manager::check_configs_(const ServerConfigs& configs) const
{
    ServerConfigs cfgs;

    for (const auto& n : configs)
    {
        for (const auto& c : cfgs)
        {
            if (n.conflicts(c))
            {
                std::stringstream ss;
                ss << "conflicting MDS configs: " << n << " vs. " << c;
                return ss.str();
            }
        }

        for (const auto& p : nodes_)
        {
            if (n != p.first and n.conflicts(p.first))
            {
                std::stringstream ss;
                ss << "unsupported MDS config change from " << p.first << " to " << n;
                return ss.str();
            }
        }

        cfgs.push_back(n);
    }

    return boost::none;
}

Manager::ConfigsAndServers
Manager::make_nodes_(const ServerConfigs& configs) const
{
    auto maybe_str(check_configs_(configs));
    if (maybe_str)
    {
        LOG_ERROR(*maybe_str);
        throw Exception(maybe_str->c_str());
    }

    ConfigsAndServers nodes;
    nodes.reserve(configs.size());

    for (const auto& c : configs)
    {
        LOG_INFO("server config " << c);
        ServerPtr s;

        for (const auto& n : nodes_)
        {
            if (c == n.first)
            {
                LOG_INFO("server " << c << " exists - reusing it");
                s = n.second;
                break;
            }
        }

        if (s == nullptr)
        {
            LOG_INFO("creating new server " << c);
            s = make_server_(c);
        }

        nodes.emplace_back(std::make_pair(c,
                                          s));
    }

    return nodes;
}

Manager::ServerPtr
Manager::make_server_(const ServerConfig& cfg) const
{
    auto db(std::make_shared<DataBase>(std::make_shared<RocksDataBase>(cfg.db_path,
                                                                       cfg.rocks_config),
                                       cm_,
                                       cfg.scratch_path,
                                       mds_cached_pages.value(),
                                       mds_poll_secs.value()));

    const size_t nthreads = mds_threads.value() ?
        mds_threads.value() :
        boost::thread::hardware_concurrency();

    boost::optional<std::chrono::seconds> timeout;
    if (mds_timeout_secs.value())
    {
        timeout = std::chrono::seconds(mds_timeout_secs.value());
    }

    return std::make_shared<ServerNG>(db,
                                      cfg.node_config.address(),
                                      cfg.node_config.port(),
                                      timeout,
                                      nthreads);
}

std::chrono::seconds
Manager::poll_interval() const
{
    return std::chrono::seconds(mds_poll_secs.value());
}

void
Manager::start_one(const ServerConfig& cfg)
{
    LOG_INFO("new config: " << cfg);

    LOCK();

    ServerConfigs configs;
    configs.reserve(nodes_.size() + 1);

    for (const auto& n : nodes_)
    {
        configs.emplace_back(n.first);
    }

    configs.emplace_back(cfg);
    nodes_ = make_nodes_(configs);
    log_nodes_("start one");
}

void
Manager::stop_one(const vd::MDSNodeConfig& cfg)
{
    LOG_INFO("victim: " << cfg);

    LOCK();

    ConfigsAndServers nodes;
    nodes.reserve(nodes_.empty() ? 0 : nodes_.size() - 1);

    for (const auto& n : nodes_)
    {
        if (n.first.node_config != cfg)
        {
            nodes.emplace_back(n);
        }
    }

    if (nodes_.size() == nodes.size())
    {
        LOG_ERROR(cfg << " not running here");
        throw Exception("server with specified config not running here",
                        boost::lexical_cast<std::string>(cfg).c_str());
    }

    nodes_ = nodes;
    log_nodes_("stop one");
}

void
Manager::log_nodes_(const char* desc) const
{
    LOG_INFO(desc << ": " << nodes_.size() << " server configs:");
    for (const auto& n : nodes_)
    {
        LOG_INFO("\t" << n.first);
    }
}

}
