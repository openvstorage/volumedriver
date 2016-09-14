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

#ifndef MDS_PYTHON_CLIENT_H_
#define MDS_PYTHON_CLIENT_H_

#include "ClientNG.h"
#include "Interface.h"

#include <string>
#include <vector>

#include <boost/optional.hpp>

#include <youtils/Logging.h>

#include "../MDSNodeConfig.h"
#include "../MetaDataBackendConfig.h"

namespace metadata_server
{

class PythonClient
{
public:
    explicit PythonClient(const volumedriver::MDSNodeConfig& cfg,
                          unsigned timeout_secs = volumedriver::MDSMetaDataBackendConfig::default_timeout_secs_);

    PythonClient(const volumedriver::MDSNodeConfig& cfg,
                 ForceRemote force_remote,
                 unsigned timeout_secs = volumedriver::MDSMetaDataBackendConfig::default_timeout_secs_);

    ~PythonClient() = default;

    void
    create_namespace(const std::string& nspace) const;

    void
    remove_namespace(const std::string& nspace) const;

    std::vector<std::string>
    list_namespaces() const;

    Role
    get_role(const std::string& nspace) const;

    void
    set_role(const std::string& nspace,
             Role,
             volumedriver::OwnerTag) const;

    volumedriver::OwnerTag
    owner_tag(const std::string& nspace) const;

    boost::optional<std::string>
    get_cork_id(const std::string& nspace) const;

    boost::optional<std::string>
    get_scrub_id(const std::string& nspace) const;

    size_t
    catch_up(const std::string& nspace,
             bool dry_run) const;

    TableCounters
    get_table_counters(const std::string& nspace,
                       bool reset) const;

private:
    DECLARE_LOGGER("MDSPythonClient");

    const volumedriver::MDSNodeConfig config_;
    const ForceRemote force_remote_;
    const std::chrono::seconds timeout_;

    TableInterfacePtr
    get_table_(const std::string& nspace) const;

    ClientNG::Ptr
    client_() const;
};
}

#endif // !MDS_PYTHON_CLIENT_H_
