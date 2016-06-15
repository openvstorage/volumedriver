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

#ifndef META_DATA_SERVER_DATA_BASE_H_
#define META_DATA_SERVER_DATA_BASE_H_

#include "Interface.h"
#include "Table.h"

#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/Logger.h>
#include <youtils/PeriodicActionPool.h>

namespace metadata_server
{

// Find a better name for this. Or even better, find a way to get rid of it, since all
// it does is housekeeping for `Table's, which wrap around `RocksTable's, which are in
// turn managed by `RocksTable'. So we have double housekeeping here.
class DataBase
    : public DataBaseInterface
{
public:
    DataBase(DataBaseInterfacePtr,
             backend::BackendConnectionManagerPtr,
             youtils::PeriodicActionPool::Ptr,
             const boost::filesystem::path& scratch_dir,
             uint32_t cached_pages,
             const std::atomic<uint64_t>& poll_secs);

    virtual ~DataBase();

    DataBase(const DataBase&) = delete;

    DataBase&
    operator=(const DataBase&) = delete;

    virtual TableInterfacePtr
    open(const std::string& nspace) override final;

    virtual void
    drop(const std::string& nspace) override final;

    virtual std::vector<std::string>
    list_namespaces() override final;

    boost::filesystem::path
    scratch_dir(const std::string& nspace) const
    {
        return scratch_dir_ / nspace;
    }

private:
    DECLARE_LOGGER("MetaDataServerDataBase");

    // protects tables_ and server_
    mutable boost::mutex lock_;

    // Represents open tables.
    std::map<std::string, TablePtr> tables_;

    DataBaseInterfacePtr db_;
    backend::BackendConnectionManagerPtr cm_;
    youtils::PeriodicActionPool::Ptr act_pool_;
    const boost::filesystem::path scratch_dir_;
    const uint32_t cached_pages_;
    const std::atomic<uint64_t>& poll_secs_;

    void
    restart_();

    TablePtr
    create_table_(const std::string& nspace,
                  const std::chrono::milliseconds ramp_up);
};

typedef std::shared_ptr<DataBase> DataBasePtr;

}

#endif //! META_DATA_SERVER_DATA_BASE_H_
