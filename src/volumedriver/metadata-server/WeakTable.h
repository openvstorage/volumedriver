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

#ifndef MDS_WEAK_TABLE_H_
#define MDS_WEAK_TABLE_H_

#include "Interface.h"

namespace metadata_server
{

class WeakTable
    : public TableInterface
{
public:
    explicit WeakTable(TableInterfacePtr table)
        : table_(table)
    {}

    virtual ~WeakTable() = default;

    WeakTable(const WeakTable&) = default;

    WeakTable&
    operator=(const WeakTable&) = default;

    virtual void
    multiset(const Records& recs,
             Barrier barrier) override final
    {
        lock_()->multiset(recs,
                          barrier);
    }

    virtual TableInterface::MaybeStrings
    multiget(const Keys& keys) override final
    {
        return lock_()->multiget(keys);
    }

    virtual void
    apply_relocations(const volumedriver::ScrubId& scrub_id,
                      const volumedriver::SCOCloneID cid,
                      const RelocationLogs& relocs) override final
    {
        return lock_()->apply_relocations(scrub_id,
                                          cid,
                                          relocs);
    }

    virtual void
    clear() override final
    {
        lock_()->clear();
    }

    virtual Role
    get_role() const override final
    {
        return lock_()->get_role();
    }

    virtual void
    set_role(Role role) override final
    {
        lock_()->set_role(role);
    }

    virtual const std::string&
    nspace() const override final
    {
        return lock_()->nspace();
    }

    virtual size_t
    catch_up(volumedriver::DryRun dry_run) override final
    {
        return lock_()->catch_up(dry_run);
    }

    virtual TableCounters
    get_counters(volumedriver::Reset reset) override final
    {
        return lock_()->get_counters(reset);
    }

    MAKE_EXCEPTION(Exception,
                   fungi::IOException);

private:
    DECLARE_LOGGER("WeakTable");

    std::weak_ptr<TableInterface> table_;

    std::shared_ptr<TableInterface>
    lock_() const
    {
        std::shared_ptr<TableInterface> table(table_.lock());
        if (table == nullptr)
        {
            LOG_ERROR("Underlying TableInterface is gone");
            throw Exception("Underlying TableInterface is gone");
        }

        return table;
    }

};

}

#endif // !MDS_WEAK_TABLE_H_
