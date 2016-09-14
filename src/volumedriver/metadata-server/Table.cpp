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

#include "Manager.h"
#include "Table.h"
#include "Utils.h"

#include <youtils/Assert.h>

#include <volumedriver/MetaDataStoreBuilder.h>
#include <volumedriver/OwnerTag.h>
#include <volumedriver/RelocationReaderFactory.h>

namespace metadata_server
{

using namespace std::string_literals;

namespace be = backend;
namespace fs = boost::filesystem;
namespace sc = std::chrono;
namespace vd = volumedriver;
namespace yt = youtils;

#define LOCKR()                                                 \
    boost::shared_lock<decltype(rwlock_)> rlg__(rwlock_)

#define LOCKW()                                                 \
    boost::unique_lock<decltype(rwlock_)> wlg__(rwlock_)

#define LOCK_OWNER()                                            \
    boost::lock_guard<decltype(owner_lock_)> olg__(owner_lock_)

#define LOCK_COUNTERS()                                         \
    boost::lock_guard<decltype(counters_lock_)> clg__(counters_lock_)

Table::Table(DataBaseInterfacePtr db,
             be::BackendInterfacePtr bi,
             yt::PeriodicActionPool::Ptr act_pool,
             const fs::path& scratch_dir,
             const uint32_t max_cached_pages,
             const std::atomic<uint64_t>& poll_secs,
             const sc::milliseconds& ramp_up)
    : db_(db)
    , table_(db_->open(bi->getNS().str()))
    , bi_(std::move(bi))
    , act_pool_(act_pool)
    , poll_secs_(poll_secs)
    , max_cached_pages_(max_cached_pages)
    , scratch_dir_(scratch_dir)
{
    VERIFY(bi_->getNS().str() == table_->nspace());

    LOG_INFO(table_->nspace() << ": new table, scratch dir " << scratch_dir_);
    start_(ramp_up);
}

Table::~Table()
{
    LOG_INFO(table_->nspace() << ": bye");
    // stop the periodic action first to prevent it from firing while
    // half the members are destructed already
    act_ = nullptr;
}

std::unique_ptr<vd::CachedMetaDataStore>
Table::make_mdstore_()
{
    auto mdbackend(std::make_shared<vd::MDSMetaDataBackend>(db_,
                                                            bi_->getNS(),
                                                            TableInterface::owner_tag()));

    return std::make_unique<vd::CachedMetaDataStore>(mdbackend,
                                                     table_->nspace(),
                                                     max_cached_pages_);
}

void
Table::start_(const sc::milliseconds& ramp_up)
{
    LOG_INFO(table_->nspace() << ": starting periodic background check");

    yt::PeriodicAction::AbortableAction a([this]() -> auto
                                          {
                                              return work_();
                                          });

    act_ = act_pool_->create_task("mds-poll-namespace-"s + table_->nspace(),
                                  std::move(a),
                                  poll_secs_,
                                  true,
                                  ramp_up);
}

Role
Table::get_role() const
{
    LOCKR();
    return TableInterface::get_role();
}

void
Table::set_role(Role role,
                vd::OwnerTag owner_tag)
{
    decltype(act_) stop_act;

    LOCKW();

    const Role old_role = TableInterface::get_role();
    LOG_INFO(table_->nspace() << ": transition from " <<
             old_role << " to " << role <<
             " requested, owner tag " << owner_tag);

    if (role != old_role)
    {

        switch (role)
        {
        case Role::Master:
            {
                stop_act = std::move(act_);
                break;
            }
        case Role::Slave:
            {
                start_(sc::milliseconds(0));
                break;
            }
        }
    }

    LOCK_OWNER();
    TableInterface::set_role(role,
                             owner_tag);
}

void
Table::update_nsid_map_(const vd::NSIDMap& nsid_map)
{
    // XXX: we might as well build the nsidmap only once - it shouldn't change
    // afterwards anymore.
    for (size_t i = 0; i < nsid_map.size(); ++i)
    {
        auto& bi_new = nsid_map.get(i);
        if (bi_new)
        {
            auto& bi_old = nsid_map_.get(i);
            if (bi_old)
            {
                VERIFY(bi_new->getNS() == bi_old->getNS());
            }
            else
            {
                nsid_map_.set(vd::SCOCloneID(i),
                              bi_new->clone());
            }
        }
    }
}

void
Table::apply_relocations(const vd::ScrubId& scrub_id,
                         const vd::SCOCloneID cid,
                         const TableInterface::RelocationLogs& relocs)
{
    LOG_INFO(table_->nspace() << ": application of relocations requested");

    LOCKW();

    // For now the application of relocs happens via 2 paths:
    // * master: MetaDataStoreInterface::applyRelocs is used as that doesn't mess
    //   with the client side cache (we'd have to drop it otherwise)
    // * slave: ends up here.
    // This does however not rule out that we encounter a table in a master role
    // here as there could have been a failover after applying relocs to the master.
    TODO("AR: reconsider application of relocations to master tables");

    auto mdstore(make_mdstore_());
    const auto old_scrub_id(mdstore->scrub_id());

    if (old_scrub_id == scrub_id)
    {
        LOG_INFO(table_->nspace() <<
                 ": relocations already applied, nothing left to do");
    }
    else if (TableInterface::get_role() == Role::Master)
    {
        LOG_ERROR(table_->nspace() <<
                  ": table is in master role but scrub IDs do not match: have " <<
                  old_scrub_id << ", got " << scrub_id);
    }
    else
    {
        vd::MetaDataStoreBuilder builder(*mdstore,
                                         bi_->clone(),
                                         scratch_dir_);

        const vd::MetaDataStoreBuilder::Result res(builder(boost::none,
                                                           vd::CheckScrubId::T));

        update_nsid_map_(res.nsid_map);
        update_counters_(res);

        if (mdstore->scrub_id() == scrub_id)
        {
            LOG_INFO(table_->nspace() <<
                     ": relocations already applied, metadata was probably rebuilt from scratch?");
        }
        else
        {
            try
            {
                vd::RelocationReaderFactory factory(relocs,
                                                    scratch_dir_,
                                                    nsid_map_.get(cid)->clone(),
                                                    vd::CombinedTLogReader::FetchStrategy::Concurrent);
                mdstore->applyRelocs(factory,
                                     cid,
                                     scrub_id);
            }
            CATCH_STD_ALL_EWHAT({
                    LOG_ERROR(table_->nspace() <<
                              ": failed to apply relocations: " << EWHAT);
                    mdstore.reset();

                    if (TableInterface::get_role() == Role::Slave)
                    {
                        LOG_ERROR(table_->nspace() <<
                                  ": table is in slave role - throwing metadata away");
                        table_->clear(vd::OwnerTag(0));
                    }
                    throw;
                });
        }
    }
}

void
Table::check_updates_permitted_(const vd::OwnerTag owner_tag,
                                const char* desc) const
{
    ASSERT_LOCKABLE_LOCKED(owner_lock_);

    if (TableInterface::get_role() == Role::Slave)
    {
        LOG_ERROR(table_->nspace() << ": updates (" << desc <<
                  ") are not permitted while in slave role");
        throw NoUpdatesOnSlavesException("Database updates are not permitted in slave role",
                                         table_->nspace().c_str());
    }

    if (owner_tag != TableInterface::owner_tag())
    {
        LOG_ERROR(table_->nspace() << ": owner tag mismatch, have " <<
                  TableInterface::owner_tag() << ", got " << owner_tag);
        throw vd::OwnerTagMismatchException("Owner tag mismatch",
                                            table_->nspace().c_str());
    }
}

void
Table::multiset(const TableInterface::Records& records,
                Barrier barrier,
                vd::OwnerTag owner_tag)
{
    // LOG_TRACE(table_->nspace() << ", " << records.size() << " records");

    LOCKR();
    LOCK_OWNER();
    check_updates_permitted_(owner_tag,
                             "multiset");
    table_->multiset(records,
                     barrier,
                     owner_tag);
}

TableInterface::MaybeStrings
Table::multiget(const TableInterface::Keys& keys)
{
    // LOG_TRACE(table_->nspace() << ", " << keys.size() << " keys");

    LOCKR();

    return table_->multiget(keys);
}

yt::PeriodicActionContinue
Table::work_()
{
    boost::upgrade_lock<decltype(rwlock_)> ulg(rwlock_);

    LOG_INFO(table_->nspace() << ": running periodic action");

    if (TableInterface::get_role() == Role::Slave)
    {
        try
        {
            auto mdstore(make_mdstore_());

            vd::MetaDataStoreBuilder builder(*mdstore,
                                             bi_->clone(),
                                             scratch_dir_);

            const vd::MetaDataStoreBuilder::Result res(builder(boost::none,
                                                               vd::CheckScrubId::F));

            boost::upgrade_to_unique_lock<decltype(rwlock_)> u(ulg);

            update_nsid_map_(res.nsid_map);
            update_counters_(res);
        }
        catch (be::BackendNamespaceDoesNotExistException& e)
        {
            LOG_WARN(table_->nspace() <<
                     ": does not exist anymore on the backend, cleaning up");
            db_->drop(table_->nspace());
            return yt::PeriodicActionContinue::F;
        }
    }
    else
    {
        LOG_TRACE(table_->nspace() << ": currently in role " <<
                  TableInterface::get_role() <<
                  " - not doing anything but awaiting dismissal in the not too distant future");
    }

    return yt::PeriodicActionContinue::T;
}

void
Table::clear(vd::OwnerTag owner_tag)
{
    LOG_INFO(table_->nspace() << ": removing all records");

    LOCKR();
    LOCK_OWNER();
    check_updates_permitted_(owner_tag,
                             "clear");
    table_->clear(owner_tag);
}

size_t
Table::catch_up(vd::DryRun dry_run)
{
    LOG_INFO(table_->nspace() << ": request to catch up; dry run: " << dry_run);

    LOCKW();

    if (TableInterface::get_role() == Role::Master)
    {
        LOG_WARN(table_->nspace() << ": cannot catch up while having a master role");
        return 0;
    }
    else
    {
        auto mdstore(make_mdstore_());

        vd::MetaDataStoreBuilder builder(*mdstore,
                                         bi_->clone(),
                                         scratch_dir_);

        const vd::MetaDataStoreBuilder::Result res(builder(boost::none,
                                                           vd::CheckScrubId::T,
                                                           dry_run));
        if (dry_run == vd::DryRun::F)
        {
            update_nsid_map_(res.nsid_map);
            update_counters_(res);
        }

        return res.num_tlogs;
    }
}

TableCounters
Table::get_counters(vd::Reset reset)
{
    LOCKR();
    LOCK_COUNTERS();

    if (reset == vd::Reset::T)
    {
        TableCounters c;
        std::swap(c, counters_);
        return c;
    }
    else
    {
        return counters_;
    }
}

void
Table::update_counters_(const vd::MetaDataStoreBuilder::Result& res)
{
    LOCK_COUNTERS();

    if (res.num_tlogs)
    {
        counters_.total_tlogs_read += res.num_tlogs;
        if (res.full_rebuild)
        {
            counters_.full_rebuilds += 1;
        }
        else
        {
            counters_.incremental_updates += 1;
        }
    }
}

}
