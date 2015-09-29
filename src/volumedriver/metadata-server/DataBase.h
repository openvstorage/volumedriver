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

#ifndef META_DATA_SERVER_DATA_BASE_H_
#define META_DATA_SERVER_DATA_BASE_H_

#include "Interface.h"
#include "Table.h"

#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/Logger.h>

namespace metadata_server
{

// Find a better name for this. Or even better, find a way to get rid of it, since all
// it does is housekeeping for `Table's, which wrap around `RocksTable's, which are in
// turn managed by `RocksTable'. So we have double housekeeping here.
class DataBase
    : public DataBaseInterface
{
public:
    DataBase(DataBaseInterfacePtr db,
             backend::BackendConnectionManagerPtr cm,
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
    const boost::filesystem::path scratch_dir_;
    const uint32_t cached_pages_;
    const std::atomic<uint64_t>& poll_secs_;

    void
    restart_();

    TablePtr
    create_table_(const std::string& nspace,
                  const boost::chrono::milliseconds ramp_up);
};

typedef std::shared_ptr<DataBase> DataBasePtr;

}

#endif //! META_DATA_SERVER_DATA_BASE_H_
