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
             Role role) const;

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
