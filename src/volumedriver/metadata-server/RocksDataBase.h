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

#ifndef META_DATA_SERVER_ROCKS_DATA_BASE_H_
#define META_DATA_SERVER_ROCKS_DATA_BASE_H_

#include "Interface.h"
#include "RocksConfig.h"
#include "RocksTable.h"

#include <map>

#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>

#include <rocksdb/options.h>

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

    virtual TableInterfacePtr
    open(const std::string& nspace);

    virtual std::vector<std::string>
    list_namespaces();

    virtual void
    drop(const std::string& nspace);

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
