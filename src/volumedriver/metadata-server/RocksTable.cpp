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

#include "RocksDataBase.h"
#include "RocksTable.h"

#include <rocksdb/slice.h>
#include <rocksdb/status.h>
#include <rocksdb/write_batch.h>

#include <youtils/Assert.h>

#include <volumedriver/CachedMetaDataPage.h>

namespace metadata_server
{

#define LOCKR()                                 \
    boost::shared_lock<decltype(rwlock_)> rlg__(rwlock_)

#define LOCKW()                                 \
    boost::unique_lock<decltype(rwlock_)> wlg__(rwlock_)

namespace be = backend;
namespace rdb = rocksdb;
namespace vd = volumedriver;
namespace yt = youtils;

#define HANDLE(s) if (not s.ok())                                       \
    {                                                                   \
        LOG_ERROR("quoth rocksdb: " << s.ToString());                   \
        throw RocksDataBaseException(s.ToString().c_str(),              \
                                     __FUNCTION__);                     \
    }

namespace
{

template<typename T>
rdb::Slice
item_to_slice(const T& t)
{
    return rdb::Slice(static_cast<const char*>(t.data),
                      t.size);
}

}

RocksTable::RocksTable(const std::string& nspace,
                       std::shared_ptr<rdb::DB>& db,
                       std::unique_ptr<rdb::ColumnFamilyHandle> column_family,
                       const RocksConfig& rocks_config)
    : db_(db)
    , column_family_(std::move(column_family))
    , column_family_options_(rocks_config.column_family_options())
    , read_options_(rocks_config.read_options())
    , write_options_(rocks_config.write_options())
    , nspace_(nspace)
{
    LOG_INFO(nspace_ << ": creating table");
}

void
RocksTable::drop()
{
    LOCKR();
    LOG_INFO(nspace_ << ": dropping the table");

    HANDLE(db_->DropColumnFamily(column_family_.get()));
}

void
RocksTable::multiset(const TableInterface::Records& records,
                     Barrier barrier,
                     vd::OwnerTag)
{
    rdb::WriteBatch batch;

    LOCKR();

    ASSERT(column_family_ != nullptr);

    for (const auto& r : records)
    {
        if (r.val.data == nullptr)
        {
            batch.Delete(column_family_.get(),
                         item_to_slice(r.key));
        }
        else
        {
            batch.Put(column_family_.get(),
                      item_to_slice(r.key),
                      item_to_slice(r.val));
        }
    }

    if (barrier == Barrier::T)
    {
        HANDLE(db_->Flush(rdb::FlushOptions(),
                          column_family_.get()));
    }

    HANDLE(db_->Write(write_options_,
                      &batch));
}

TableInterface::MaybeStrings
RocksTable::multiget(const TableInterface::Keys& keys)
{
    std::vector<rdb::Slice> keyv;
    keyv.reserve(keys.size());

    for (const auto& k : keys)
    {
        keyv.emplace_back(item_to_slice(k));
    }

    std::vector<std::string> valv;

    LOCKR();

    ASSERT(column_family_ != nullptr);
    const std::vector<rdb::ColumnFamilyHandle*> handles(keys.size(),
                                                        column_family_.get());

    const std::vector<rdb::Status> statv(db_->MultiGet(read_options_,
                                                       handles,
                                                       keyv,
                                                       &valv));

    VERIFY(statv.size() == keyv.size());
    VERIFY(keyv.size() == valv.size());

    TableInterface::MaybeStrings vals;
    vals.reserve(statv.size());

    for (size_t i = 0; i < statv.size(); ++i)
    {
        switch (statv[i].code())
        {
        case rdb::Status::kOk:
            vals.emplace_back(std::move(valv[i]));
            break;
        case rdb::Status::kNotFound:
            vals.emplace_back(boost::none);
            break;
        default:
            HANDLE(statv[i]);
        }
    }

    return vals;
}

void
RocksTable::apply_relocations(const vd::ScrubId&,
                              const vd::SCOCloneID,
                              const TableInterface::RelocationLogs&)
{
    VERIFY(0 == "RocksTable::apply_relocations shouldn't be invoked");
}

void
RocksTable::clear(vd::OwnerTag)
{
    // The best (? fastest!) way to clear all records seems to be to drop the column family
    // and create a new one of the same name. For that we need exclusive access to the
    // ColumnFamilyHandle.
    LOCKW();

    HANDLE(db_->DropColumnFamily(column_family_.get()));

    // Rather take down the thing by dereferencing a nullptr than continuing with the
    // old handle as that spells data loss once its closed.
    // It's ok if we crash here since a new column family will be created on next open.
    column_family_.reset();

    rdb::ColumnFamilyHandle* h;

    HANDLE(db_->CreateColumnFamily(column_family_options_,
                                   nspace_,
                                   &h));

    column_family_.reset(h);
}


// XXX: these two could suggest that RocksTable implementing TableInterface might not be a good idea after all?
size_t
RocksTable::catch_up(vd::DryRun)
{
    VERIFY(0 == "RocksTable::catch_up shouldn't be invoked");
}

TableCounters
RocksTable::get_counters(vd::Reset)
{
    VERIFY(0 == "RocksTable::get_table_counters shouldn't be invoked");
}

}
