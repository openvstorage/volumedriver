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

#ifndef META_DATA_SERVER_TABLE_H_
#define META_DATA_SERVER_TABLE_H_

#include "Interface.h"

#include <memory>

#include <boost/chrono.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/PeriodicActionPool.h>

#include <backend/BackendInterface.h>

#include <volumedriver/CachedMetaDataStore.h>
#include <volumedriver/MDSMetaDataBackend.h>
#include <volumedriver/MetaDataStoreBuilder.h>
#include <volumedriver/NSIDMap.h>
#include <volumedriver/ScrubId.h>

namespace metadata_server
{

// Idea:
// A table can have either of 2 roles:
// * Slave: a periodic action checks the metadata on the backend
//   * if a new cork is detected the underlying TableInterface is brought to that state
//   * if an older cork is detected there was a snapshot rollback and the metadata is
//      rebuilt from scratch
//   * if the namespace is gone, self-destruction is initiated
//   . Callers are not permitted to update the underlying TableInterface via multiset.
//   Scrub application is allowed.
// * Master: no background action; all calls are routed through to the underlying
//   TableInterface.
//
// Roles are switched explicitly via ->set_role().
//
// Locking:
//   A shared mutex is held exclusively
//   * while switching roles (*)
//   * when applying scrub results
//   * when being forced to catch up
//   .
//   All other calls use it in shared mode.
//
//   The counters are protected by a plain mutex which needs to be grabbed *after* the shared one.
//   The Role / OwnerTag are protected by a plain mutex which needs to be grapped *after* the shared
//   one to check ownership when doing updates.
class Table
    : public TableInterface
{
public:
    MAKE_EXCEPTION(Exception, fungi::IOException);
    MAKE_EXCEPTION(NoUpdatesOnSlavesException, Exception);

    Table(DataBaseInterfacePtr db,
          backend::BackendInterfacePtr bi,
          youtils::PeriodicActionPool::Ptr,
          const boost::filesystem::path& scratch_dir,
          const uint32_t max_cached_pages,
          const std::atomic<uint64_t>& poll_secs,
          const std::chrono::milliseconds& ramp_up =
          std::chrono::milliseconds(0));

    virtual ~Table();

    Table(const Table&) = delete;

    Table&
    operator=(const Table&) = delete;

    virtual void
    apply_relocations(const volumedriver::ScrubId&,
                      const volumedriver::SCOCloneID,
                      const TableInterface::RelocationLogs&) override final;

    virtual void
    multiset(const TableInterface::Records&,
             Barrier,
             volumedriver::OwnerTag) override final;

    virtual TableInterface::MaybeStrings
    multiget(const TableInterface::Keys&) override final;

    virtual Role
    get_role() const override final;

    virtual void
    set_role(Role,
             volumedriver::OwnerTag) override final;

    virtual const std::string&
    nspace() const override final
    {
        return table_->nspace();
    }

    virtual void
    clear(volumedriver::OwnerTag) override final;

    virtual size_t
    catch_up(volumedriver::DryRun) override final;

    virtual TableCounters
    get_counters(volumedriver::Reset) override final;

    void
    stop();

private:
    DECLARE_LOGGER("MetaDataServerTable");

    mutable boost::shared_mutex rwlock_;
    mutable boost::mutex owner_lock_;

    DataBaseInterfacePtr db_;
    TableInterfacePtr table_;
    backend::BackendInterfacePtr bi_;

    youtils::PeriodicActionPool::Ptr act_pool_;
    std::unique_ptr<youtils::PeriodicActionPool::Task> act_;
    const std::atomic<uint64_t>& poll_secs_;

    const uint32_t max_cached_pages_;
    const boost::filesystem::path scratch_dir_;

    volumedriver::NSIDMap nsid_map_;

    boost::mutex counters_lock_;
    TableCounters counters_;

    void
    start_(const std::chrono::milliseconds& ramp_up);

    void
    drop_and_stop_();

    youtils::PeriodicActionContinue
    work_();

    std::unique_ptr<volumedriver::CachedMetaDataStore>
    make_mdstore_();

    void
    update_nsid_map_(const volumedriver::NSIDMap&);

    void
    check_updates_permitted_(const volumedriver::OwnerTag,
                             const char* desc) const;

    void
    update_counters_(const volumedriver::MetaDataStoreBuilder::Result&);
};

typedef std::shared_ptr<Table> TablePtr;

}

#endif // !META_DATA_SERVER_TABLE_H_
