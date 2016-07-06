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

#ifndef VFS_LOCAL_PYTHON_CLIENT_H_
#define VFS_LOCAL_PYTHON_CLIENT_H_

#include "PythonClient.h"

#include <boost/filesystem.hpp>

#include <string>
#include <vector>

#include <youtils/ConfigFetcher.h>
#include <youtils/Logger.h>
#include <youtils/Logging.h>
#include <youtils/UpdateReport.h>

namespace volumedriverfs
{

class LocalPythonClient final
    : public PythonClient
{
public:
    explicit LocalPythonClient(const std::string& config);

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

    std::vector<volumedriver::ClusterCacheHandle>
    list_cluster_cache_handles();

    XMLRPCClusterCacheHandleInfo
    get_cluster_cache_handle_info(const volumedriver::ClusterCacheHandle);

    void
    remove_cluster_cache_handle(const volumedriver::ClusterCacheHandle);

private:
    DECLARE_LOGGER("LocalPythonClient");

    youtils::ConfigFetcher config_fetcher_;
};

}

#endif // !VFS_LOCAL_PYTHON_CLIENT_H_
