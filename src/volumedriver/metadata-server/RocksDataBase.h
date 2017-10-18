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

#ifndef META_DATA_SERVER_ROCKS_DATA_BASE_H_
#define META_DATA_SERVER_ROCKS_DATA_BASE_H_

#include "Interface.h"
#include "RocksConfig.h"
#include "RocksTable.h"

#include <map>

#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>

#include <rocksdb/options.h>

#include <youtils/CreateIfNecessary.h>
#include <youtils/Logging.h>

namespace metadata_server
{

class RocksDataBase
    : public DataBaseInterface
{
public:
    explicit RocksDataBase(const boost::filesystem::path& path,
                           const RocksConfig& = RocksConfig());

    virtual ~RocksDataBase();

    RocksDataBase(const RocksDataBase&) = delete;

    RocksDataBase&
    operator=(const RocksDataBase&) = delete;

    TableInterfacePtr
    open(const std::string& nspace) override final
    {
        return open(nspace,
                    CreateIfNecessary::T);
    }

    RocksTablePtr
    open(const std::string& nspace,
         CreateIfNecessary);

    std::vector<std::string>
    list_namespaces() override final;

    void
    drop(const std::string& nspace) override final;

private:
    DECLARE_LOGGER("MetaDataServerRocksDataBase");

    // protects tables;
    mutable boost::mutex lock_;
    std::shared_ptr<rocksdb::DB> db_;
    std::map<std::string, RocksTablePtr> tables_;
    RocksConfig rocks_config_;

    RocksTablePtr
    make_table_(const std::string& nspace,
                rocksdb::ColumnFamilyHandle* h);
};

typedef std::shared_ptr<RocksDataBase> RocksDataBasePtr;

}

#endif // !META_DATA_SERVER_ROCKS_DATA_BASE_H_
