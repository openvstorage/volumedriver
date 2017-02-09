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
    using MaybeSeconds = PythonClient::MaybeSeconds;

    explicit LocalPythonClient(const std::string& config,
                               const MaybeSeconds& = boost::none);

    ~LocalPythonClient() = default;

    LocalPythonClient(const LocalPythonClient&) = default;

    LocalPythonClient&
    operator=(const LocalPythonClient&) = default;

    void
    destroy();

    std::string
    get_running_configuration(bool report_defaults = false,
                              const MaybeSeconds& = boost::none);

    youtils::UpdateReport
    update_configuration(const std::string& path,
                         const MaybeSeconds& = boost::none);

    void
    set_general_logging_level(youtils::Severity,
                              const MaybeSeconds& = boost::none);

    youtils::Severity
    get_general_logging_level(const MaybeSeconds& = boost::none);

    std::vector<youtils::Logger::filter_t>
    get_logging_filters(const MaybeSeconds& = boost::none);

    void
    add_logging_filter(const std::string& match,
                       youtils::Severity sev,
                       const MaybeSeconds& = boost::none);

    void
    remove_logging_filter(const std::string& match,
                          const MaybeSeconds& = boost::none);

    void
    remove_logging_filters(const MaybeSeconds& = boost::none);

    std::string
    malloc_info(const MaybeSeconds& = boost::none);

    std::vector<volumedriver::ClusterCacheHandle>
    list_cluster_cache_handles(const MaybeSeconds& = boost::none);

    XMLRPCClusterCacheHandleInfo
    get_cluster_cache_handle_info(const volumedriver::ClusterCacheHandle,
                                  const MaybeSeconds& = boost::none);

    void
    remove_cluster_cache_handle(const volumedriver::ClusterCacheHandle,
                                const MaybeSeconds& = boost::none);

    void
    remove_namespace_from_sco_cache(const std::string&,
                                    const MaybeSeconds& = boost::none);

private:
    DECLARE_LOGGER("LocalPythonClient");

    std::unique_ptr<youtils::ConfigFetcher> config_fetcher_;
};

}

#endif // !VFS_LOCAL_PYTHON_CLIENT_H_
