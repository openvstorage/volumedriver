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

#ifndef VFS_LOCAL_PYTHON_CLIENT_H_
#define VFS_LOCAL_PYTHON_CLIENT_H_

#include "PythonClient.h"

#include <boost/filesystem.hpp>

#include <string>
#include <vector>

#include <youtils/Logger.h>
#include <youtils/Logging.h>
#include <youtils/UpdateReport.h>

namespace volumedriverfs
{

class LocalPythonClient final
    : public PythonClient
{
public:
    LocalPythonClient(const std::string& config_file);

    ~LocalPythonClient() = default;

    LocalPythonClient(const LocalPythonClient&) = default;

    LocalPythonClient&
    operator=(const LocalPythonClient&) = default;

    void
    destroy();

    std::string
    get_running_configuration(bool report_defaults = false);

    youtils::UpdateReport
    update_configuration(const std::string& path);

    void
    set_general_logging_level(youtils::Severity sev);

    youtils::Severity
    get_general_logging_level();

    std::vector<youtils::Logger::filter_t>
    get_logging_filters();

    void
    add_logging_filter(const std::string& match,
                       youtils::Severity sev);

    void
    remove_logging_filter(const std::string& match);

    void
    remove_logging_filters();

    std::string
    malloc_info();

    void
    stop_object(const std::string& id,
                bool delete_local_data = true);

    void
    restart_object(const std::string& id,
                   bool force_restart);

    std::vector<volumedriver::ClusterCacheHandle>
    list_cluster_cache_handles();

    XMLRPCClusterCacheHandleInfo
    get_cluster_cache_handle_info(const volumedriver::ClusterCacheHandle);

    void
    remove_cluster_cache_handle(const volumedriver::ClusterCacheHandle);

private:
    DECLARE_LOGGER("LocalPythonClient");

    const boost::filesystem::path config_file_;
};

}

#endif // !VFS_LOCAL_PYTHON_CLIENT_H_
