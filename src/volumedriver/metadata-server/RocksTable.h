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

#ifndef META_DATA_SERVER_ROCKS_TABLE_H_
#define META_DATA_SERVER_ROCKS_TABLE_H_

#include "Interface.h"
#include "RocksConfig.h"

#include <memory>

#include <boost/thread/shared_mutex.hpp>

#include <rocksdb/db.h>

#include <youtils/IOException.h>
#include <youtils/Logging.h>

#include <backend/Namespace.h>

namespace metadata_server
{

class RocksTable
    : public TableInterface
{
public:
    RocksTable(const std::string& nspace,
               std::shared_ptr<rocksdb::DB>& db,
               std::unique_ptr<rocksdb::ColumnFamilyHandle> column_family,
               const RocksConfig& rocks_config);

    virtual ~RocksTable() = default;

    RocksTable(const RocksTable&) = delete;

    RocksTable&
    operator=(const RocksTable&) = delete;

    void
    drop();

    virtual void
    multiset(const TableInterface::Records& sets,
             Barrier barrier) override final;

    virtual TableInterface::MaybeStrings
    multiget(const TableInterface::Keys& keys) override final;

    virtual const std::string&
    nspace() const override final
    {
        return nspace_;
    }

    virtual void
    apply_relocations(const volumedriver::ScrubId& scrub_id,
                      const volumedriver::SCOCloneID cid,
                      const TableInterface::RelocationLogs& relocs) override final;

    virtual void
    clear() override final;

    virtual size_t
    catch_up(volumedriver::DryRun dry_run) override final;

private:
    DECLARE_LOGGER("MetaDataServerRocksTable");

    // Locking (around the ColumnFamilyHandle):
    // * exclusive access during clear() - cf. comment in its implementation
    // * shared access otherwise.
    mutable boost::shared_mutex rwlock_;
    std::shared_ptr<rocksdb::DB> db_;
    std::unique_ptr<rocksdb::ColumnFamilyHandle> column_family_;
    // Getting the options from the old handle leads to a use-after-free when
    // re-creating the column family, so we keep our own copy of it.
    const rocksdb::ColumnFamilyOptions column_family_options_;
    const rocksdb::ReadOptions read_options_;
    const rocksdb::WriteOptions write_options_;
    const std::string nspace_;
};

typedef std::shared_ptr<RocksTable> RocksTablePtr;

MAKE_EXCEPTION(RocksDataBaseException,
               fungi::IOException);

}

#endif // !META_DATA_SERVER_ROCKS_TABLE_H_
