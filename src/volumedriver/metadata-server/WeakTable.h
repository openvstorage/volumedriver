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
             Barrier barrier,
             volumedriver::OwnerTag owner_tag) override final
    {
        lock_()->multiset(recs,
                          barrier,
                          owner_tag);
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
    clear(volumedriver::OwnerTag owner_tag) override final
    {
        lock_()->clear(owner_tag);
    }

    virtual Role
    get_role() const override final
    {
        return lock_()->get_role();
    }

    virtual void
    set_role(Role role,
             volumedriver::OwnerTag owner_tag) override final
    {
        lock_()->set_role(role,
                          owner_tag);
    }

    virtual volumedriver::OwnerTag
    owner_tag() const override final
    {
        return lock_()->owner_tag();
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
