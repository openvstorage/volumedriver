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

PythonClient::PythonClient(const vd::MDSNodeConfig& cfg,
                           unsigned timeout_secs)
    : PythonClient(cfg,
                   ForceRemote::F,
                   timeout_secs)
{}

PythonClient::PythonClient(const vd::MDSNodeConfig& cfg,
                           ForceRemote force_remote,
                           unsigned timeout_secs)
    : config_(cfg)
    , force_remote_(force_remote)
    , timeout_(timeout_secs)
{
    if (timeout_secs == 0)
    {
        LOG_ERROR("Timeout of 0 seconds is not supported");
        throw fungi::IOException("timeout of 0 seconds is not supported");
    }
}

ClientNG::Ptr
PythonClient::client_() const
{
    return ClientNG::create(config_,
                            0,
                            timeout_,
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
                       Role role,
                       vd::OwnerTag owner_tag) const
{
    get_table_(nspace)->set_role(role,
                                 owner_tag);
}

boost::optional<std::string>
PythonClient::get_cork_id(const std::string& nspace) const
{
    // Only for the side effect of checking namespace existence. YUCK!
    get_table_(nspace);

    vd::MDSMetaDataBackend backend(config_,
                                   be::Namespace(nspace),
                                   boost::none,
                                   timeout_);
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
                                   be::Namespace(nspace),
                                   boost::none,
                                   timeout_);
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

TableCounters
PythonClient::get_table_counters(const std::string& nspace,
                                 bool reset) const
{
    return get_table_(nspace)->get_counters(reset ?
                                            vd::Reset::T :
                                            vd::Reset::F);
}

vd::OwnerTag
PythonClient::owner_tag(const std::string& nspace) const
{
    return get_table_(nspace)->owner_tag();
}

}
