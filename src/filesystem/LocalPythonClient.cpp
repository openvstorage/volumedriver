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

#include "ConfigHelper.h"
#include "FileSystem.h"
#include "LocalPythonClient.h"
#include "XMLRPCKeys.h"
#include "XMLRPCUtils.h"

#include <boost/serialization/variant.hpp>
#include <boost/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <youtils/Logging.h>

namespace volumedriverfs
{

namespace vd = volumedriver;
namespace yt = youtils;

LocalPythonClient::LocalPythonClient(const std::string& config_file)
    : config_file_(config_file)
{
    boost::property_tree::ptree pt;
    boost::property_tree::json_parser::read_json(config_file, pt);
    const ConfigHelper argument_helper(pt);
    cluster_id_ = argument_helper.cluster_id();
    cluster_contacts_.emplace_back(argument_helper.localnode_config().host,
                                   argument_helper.localnode_config().xmlrpc_port);
}

void
LocalPythonClient::destroy()
{
    boost::property_tree::ptree pt;
    boost::property_tree::json_parser::read_json(config_file_.string(), pt);
    FileSystem::destroy(pt);
}

std::string
LocalPythonClient::get_running_configuration(bool report_default)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::show_defaults] = report_default ? "true" : "false";

    return call(PersistConfigurationToString::method_name(),
                req)[XMLRPCKeys::configuration];
}

namespace
{

struct ReportVisitor
    : public boost::static_visitor<>
{
    DECLARE_LOGGER("UpdateConfigurationReportVisitor");

    void
    operator()(const yt::ConfigurationReport& reports)
    {
        for (const auto& report : reports)
        {
            LOG_ERROR("Error updating " <<
                      report.component_name() << "/" <<
                      report.param_name() << ": " <<
                      report.problem());
        }

        throw clienterrors::ConfigurationUpdateException("error updating configuration");
    }

    void
    operator()(const yt::UpdateReport& u)
    {
        update_report = u;
    }

    yt::UpdateReport update_report;
};

}

yt::UpdateReport
LocalPythonClient::update_configuration(const std::string& path)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::configuration_path] = path;
    auto rsp(call(UpdateConfiguration::method_name(), req));
    const auto res(XMLRPCStructs::deserialize_from_xmlrpc_value<boost::variant<yt::UpdateReport,
                   yt::ConfigurationReport>>(rsp));

    ReportVisitor v;
    boost::apply_visitor(v, res);
    return v.update_report;
}

void
LocalPythonClient::set_general_logging_level(yt::Severity sev)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::log_filter_level] = boost::lexical_cast<std::string>(sev);
    call(SetGeneralLoggingLevel::method_name(), req);
}

yt::Severity
LocalPythonClient::get_general_logging_level()
{
    XmlRpc::XmlRpcValue req;
    auto rsp(call(GetGeneralLoggingLevel::method_name(), req));
    return boost::lexical_cast<yt::Severity>(static_cast<std::string&>(rsp));
}

std::vector<yt::Logger::filter_t>
LocalPythonClient::get_logging_filters()
{
    XmlRpc::XmlRpcValue req;
    auto rsp(call(ShowLoggingFilters::method_name(), req));

    std::vector<yt::Logger::filter_t> res;
    res.reserve(rsp.size());

    for (auto i = 0; i < rsp.size(); ++i)
    {
        LOG_TRACE("filter " << i);

        const std::string match = rsp[i][XMLRPCKeys::log_filter_name];
        const std::string sev = rsp[i][XMLRPCKeys::log_filter_level];

        LOG_TRACE("match " << match << ", severity " << sev);

        res.emplace_back(yt::Logger::filter_t(match,
                                              boost::lexical_cast<yt::Severity>(sev)));
    }

    return res;
}

void
LocalPythonClient::add_logging_filter(const std::string& match,
                                      yt::Severity sev)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::log_filter_name] = match;
    req[XMLRPCKeys::log_filter_level] = boost::lexical_cast<std::string>(sev);

    call(AddLoggingFilter::method_name(), req);
}

void
LocalPythonClient::remove_logging_filter(const std::string& match)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::log_filter_name] = match;

    call(RemoveLoggingFilter::method_name(), req);
}

void
LocalPythonClient::remove_logging_filters()
{
    XmlRpc::XmlRpcValue req;
    call(RemoveAllLoggingFilters::method_name(), req);
}

std::string
LocalPythonClient::malloc_info()
{
    XmlRpc::XmlRpcValue req;
    return call(MallocInfo::method_name(),
                req);
}

std::vector<vd::ClusterCacheHandle>
LocalPythonClient::list_cluster_cache_handles()
{
    XmlRpc::XmlRpcValue req;
    auto rsp(call(ListClusterCacheHandles::method_name(), req));
    std::vector<vd::ClusterCacheHandle> v;
    v.reserve(rsp.size());

    for (auto i = 0; i < rsp.size(); ++i)
    {
        const std::string s(rsp[i]);
        v.push_back(boost::lexical_cast<vd::ClusterCacheHandle>(s));
    }

    return v;
}

XMLRPCClusterCacheHandleInfo
LocalPythonClient::get_cluster_cache_handle_info(const vd::ClusterCacheHandle handle)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::cluster_cache_handle] = boost::lexical_cast<std::string>(handle);

    auto rsp(call(GetClusterCacheHandleInfo::method_name(), req));
    return
        XMLRPCStructs::deserialize_from_xmlrpc_value<XMLRPCClusterCacheHandleInfo>(rsp);
}

void
LocalPythonClient::remove_cluster_cache_handle(const vd::ClusterCacheHandle handle)
{
    XmlRpc::XmlRpcValue req;
    req[XMLRPCKeys::cluster_cache_handle] = boost::lexical_cast<std::string>(handle);

    call(RemoveClusterCacheHandle::method_name(), req);
}

}
