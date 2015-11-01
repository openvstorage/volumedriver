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

#include "ClientNG.h"
#include "PythonClient.h"
#include "../MDSMetaDataBackend.h"

#include <youtils/Assert.h>

#include <backend/Namespace.h>

namespace metadata_server
{

namespace be = backend;
namespace vd = volumedriver;
namespace yt = youtils;

PythonClient::PythonClient(const vd::MDSNodeConfig& cfg)
    : PythonClient(cfg,
                   ForceRemote::F)
{}

PythonClient::PythonClient(const vd::MDSNodeConfig& cfg,
                           ForceRemote force_remote)
    : config_(cfg)
    , force_remote_(force_remote)
{}

ClientNG::Ptr
PythonClient::client_() const
{
    return ClientNG::create(config_,
                            0,
                            boost::none,
                            force_remote_);
}

void
PythonClient::create_namespace(const std::string& nspace) const
{
    client_()->open(nspace);
}

void
PythonClient::remove_namespace(const std::string& nspace) const
{
    // Only for the side effect of checking namespace existence. YUCK!
    get_table_(nspace);
    client_()->drop(nspace);
}


std::vector<std::string>
PythonClient::list_namespaces() const
{
    return client_()->list_namespaces();
}

TableInterfacePtr
PythonClient::get_table_(const std::string& nspace) const
{
    TODO("AR: this should go away - add a flag to the client->open() call that prevents table creation");

    ClientNG::Ptr client(client_());

    const auto nspaces(client->list_namespaces());
    auto it = std::find(nspaces.begin(),
                        nspaces.end(),
                        nspace);
    if (it != nspaces.end())
    {
        TableInterfacePtr table(client->open(nspace));
        VERIFY(table != nullptr);
        return table;
    }
    else
    {
        LOG_ERROR("Namespace " << nspace << " does not exist on " << config_);
        throw fungi::IOException("Namespace does not exist");
    }
}

Role
PythonClient::get_role(const std::string& nspace) const
{
    return get_table_(nspace)->get_role();
}

void
PythonClient::set_role(const std::string& nspace,
                       Role role) const
{
    get_table_(nspace)->set_role(role);
}

boost::optional<std::string>
PythonClient::get_cork_id(const std::string& nspace) const
{
    // Only for the side effect of checking namespace existence. YUCK!
    get_table_(nspace);

    vd::MDSMetaDataBackend backend(config_,
                                   be::Namespace(nspace));
    const boost::optional<yt::UUID> cork(backend.lastCorkUUID());
    if (cork != boost::none)
    {
        return cork->str();
    }
    else
    {
        return boost::none;
    }
}

boost::optional<std::string>
PythonClient::get_scrub_id(const std::string& nspace) const
{
    // Only for the side effect of checking namespace existence. YUCK!
    get_table_(nspace);

    vd::MDSMetaDataBackend backend(config_,
                                   be::Namespace(nspace));
    const vd::MaybeScrubId scrub_id(backend.scrub_id());
    if (scrub_id != boost::none)
    {
        return static_cast<const yt::UUID&>(*scrub_id).str();
    }
    else
    {
        return boost::none;
    }
}

size_t
PythonClient::catch_up(const std::string& nspace,
                       bool dry_run) const
{
    return get_table_(nspace)->catch_up(dry_run ?
                                        vd::DryRun::T :
                                        vd::DryRun::F);
}

}
