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

#ifndef META_DATA_SERVER_TABLE_H_
#define META_DATA_SERVER_TABLE_H_

#include "Interface.h"

#include <memory>

#include <boost/chrono.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/PeriodicAction.h>

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
class Table
    : public TableInterface
{
public:
    MAKE_EXCEPTION(Exception, fungi::IOException);
    MAKE_EXCEPTION(NoUpdatesOnSlavesException, Exception);

    Table(DataBaseInterfacePtr db,
          backend::BackendInterfacePtr bi,
          const boost::filesystem::path& scratch_dir,
          const uint32_t max_cached_pages,
          const std::atomic<uint64_t>& poll_secs,
          const boost::chrono::milliseconds& ramp_up =
          boost::chrono::milliseconds(0));

    virtual ~Table();

    Table(const Table&) = delete;

    Table&
    operator=(const Table&) = delete;

    virtual void
    apply_relocations(const volumedriver::ScrubId&,
                      const volumedriver::SCOCloneID,
                      const TableInterface::RelocationLogs&) override final;

    virtual void
    multiset(const TableInterface::Records& records,
             Barrier barrier) override final;

    virtual TableInterface::MaybeStrings
    multiget(const TableInterface::Keys& keys) override final;

    virtual Role
    get_role() const override final;

    virtual void
    set_role(Role role) override final;

    virtual const std::string&
    nspace() const override final
    {
        return table_->nspace();
    }

    virtual void
    clear() override final;

    virtual size_t
    catch_up(volumedriver::DryRun dry_run) override final;

    void
    stop();

private:
    DECLARE_LOGGER("MetaDataServerTable");

    mutable boost::shared_mutex rwlock_;

    DataBaseInterfacePtr db_;
    TableInterfacePtr table_;
    backend::BackendInterfacePtr bi_;

    std::unique_ptr<youtils::PeriodicAction> act_;
    const std::atomic<uint64_t>& poll_secs_;

    const uint32_t max_cached_pages_;
    const boost::filesystem::path scratch_dir_;

    volumedriver::NSIDMap nsid_map_;

    void
    start_(const boost::chrono::milliseconds& ramp_up);

    void
    drop_and_stop_();

    youtils::PeriodicActionContinue
    work_();

    std::unique_ptr<volumedriver::CachedMetaDataStore>
    make_mdstore_();

    void
    update_nsid_map_(const volumedriver::NSIDMap&);

    void
    prevent_updates_on_slaves_(const char* desc);
};

typedef std::shared_ptr<Table> TablePtr;

}

#endif // !META_DATA_SERVER_TABLE_H_
