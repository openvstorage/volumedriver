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

#include "DataBase.h"
#include "Table.h"

#include <youtils/SourceOfUncertainty.h>

namespace metadata_server
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace sc = std::chrono;
namespace yt = youtils;

#define LOCK()                                          \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

DataBase::DataBase(DataBaseInterfacePtr db,
                   be::BackendConnectionManagerPtr cm,
                   yt::PeriodicActionPool::Ptr act_pool,
                   const fs::path& scratch_dir,
                   uint32_t cached_pages,
                   const std::atomic<uint64_t>& poll_secs)
    : db_(db)
    , cm_(cm)
    , act_pool_(act_pool)
    , scratch_dir_(scratch_dir)
    , cached_pages_(cached_pages)
    , poll_secs_(poll_secs)
{
    VERIFY(cm != nullptr);
    restart_();
}

DataBase::~DataBase()
{
    LOG_INFO("Releasing all open tables (" << tables_.size() << ")");

    LOCK();
    tables_.clear();
}

void
DataBase::restart_()
{
    LOG_INFO("Checking if there's anything to restart");

    LOCK();

    VERIFY(tables_.empty());

    const std::vector<std::string> nspaces(db_->list_namespaces());
    yt::SourceOfUncertainty rand;
    const uint64_t max_ramp_up = poll_secs_.load() * 1000;

    for (const auto& n : nspaces)
    {
        VERIFY(tables_.find(n) == tables_.end());
        const sc::milliseconds ramp_up(rand(max_ramp_up));

        LOG_INFO("restarting " << n << " with a ramp up of " << ramp_up.count() <<
                 " milliseconds");

        create_table_(n,
                      ramp_up);
    }
}

TablePtr
DataBase::create_table_(const std::string& nspace,
                        const sc::milliseconds ramp_up)
{
    auto table(std::make_shared<Table>(db_,
                                       cm_->newBackendInterface(be::Namespace(nspace)),
                                       act_pool_,
                                       scratch_dir(nspace),
                                       cached_pages_,
                                       poll_secs_,
                                       ramp_up));
    tables_[nspace] = table;
    return table;
}

TableInterfacePtr
DataBase::open(const std::string& nspace)
{
    // LOG_TRACE("opening " << nspace);

    LOCK();

    TablePtr table;

    auto it = tables_.find(nspace);
    if (it != tables_.end())
    {
        table = it->second;
    }
    else
    {
        table = create_table_(nspace,
                              sc::milliseconds(0));
    }

    VERIFY(table != nullptr);

    return table;
}

void
DataBase::drop(const std::string& nspace)
{
    LOG_INFO("dropping " << nspace);
    LOCK();

    db_->drop(nspace);
    tables_.erase(nspace);
}

std::vector<std::string>
DataBase::list_namespaces()
{
    LOCK();

    std::vector<std::string> nspaces;
    nspaces.reserve(tables_.size());

    for (const auto& p : tables_)
    {
        nspaces.emplace_back(p.first);
    }

    return nspaces;
}


}
