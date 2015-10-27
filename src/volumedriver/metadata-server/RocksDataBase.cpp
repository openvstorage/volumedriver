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

#include "RocksDataBase.h"

#include <rocksdb/options.h>
#include <rocksdb/status.h>

#include <youtils/RocksLogger.h>

#include <volumedriver/CachedMetaDataPage.h>

namespace metadata_server
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace rdb = rocksdb;
namespace vd = volumedriver;
namespace yt = youtils;

#define HANDLE(s) if (not s.ok())                                       \
    {                                                                   \
        LOG_ERROR("quoth rocksdb: " << s.ToString());                   \
        throw RocksDataBaseException(s.ToString().c_str(),              \
                                     __FUNCTION__);                     \
    }

#define LOCK()                                  \
    boost::lock_guard<decltype(lock_)> lg__(lock_);

namespace
{

const std::string default_column_family("default");

}

RocksDataBase::RocksDataBase(const fs::path& path,
                             const RocksConfig& rocks_config)
    : rocks_config_(rocks_config)
{
    rdb::DB* db;

    std::vector<std::string> family_names;

    const rdb::DBOptions db_opts(rocks_config_.db_options(path.string()));

    // Dear reader, you probably ask yourself why this explicit check is here
    // instead of having fs::create_directories throw eventually:
    // The reason is that asan (clang 3.6.0) throws a fit *in a subsequent test*,
    // complaining about a stack buffer overflow there (it doesn't when the second
    // test is run in isolation):
    //
    // arne@blackadder:~/Projects/CloudFounders/dev/volumedriver-core$ target/clang/bin/volumedriver_test --disable-logging --gtest_filter=RocksTest.hidden*:RocksTest*construction_with_path*
    // Note: Google Test filter = RocksTest.hidden*:RocksTest*construction_with_path*
    // [==========] Running 2 tests from 1 test case.
    // [----------] Global test environment set-up.
    // [----------] 2 tests from RocksTest
    // [ RUN      ] RocksTest.construction_with_path_that_is_not_a_directory
    // [       OK ] RocksTest.construction_with_path_that_is_not_a_directory (11 ms)
    // [ RUN      ] RocksTest.hidden_default_table
    // =================================================================
    // ==11628==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7fff4aef73d0 at pc 0x00000457994d bp 0x7fff4aef6270 sp 0x7fff4aef6268
    // WRITE of size 8 at 0x7fff4aef73d0 thread T0
    // [...]
    //
    // Adding the explicit check fixes that - the reason is beyond this poor
    // developer's comprehension.
    if (fs::exists(path) and not fs::is_directory(path))
    {
        LOG_ERROR(path << ": is not a directory!?");
        throw RocksDataBaseException("Path is not a directory",
                                     path.string().c_str());
    }

    if (fs::exists(path) and not fs::is_empty(path))
    {
        LOG_INFO("restarting existing RocksDB " << path);

        HANDLE(rdb::DB::ListColumnFamilies(db_opts,
                                           path.string(),
                                           &family_names));
    }
    else
    {
        LOG_INFO("creating new RocksDB " << path);

        fs::create_directories(path);
        family_names.push_back(rdb::kDefaultColumnFamilyName);
    }

    const rdb::ColumnFamilyOptions family_opts(rocks_config_.column_family_options());

    std::vector<rdb::ColumnFamilyDescriptor> family_descs;
    family_descs.reserve(family_names.size());

    for (const auto& n : family_names)
    {
        LOG_INFO("found column family " << n);
        family_descs.emplace_back(rdb::ColumnFamilyDescriptor(n,
                                                              family_opts));
    }

    std::vector<rdb::ColumnFamilyHandle*> family_handles;

    HANDLE(rdb::DB::Open(db_opts,
                         path.string(),
                         family_descs,
                         &family_handles,
                         &db));

    db_.reset(db);

    VERIFY(family_handles.size() == family_descs.size());

    for (size_t i = 0; i < family_descs.size(); ++i)
    {
        if (family_descs[i].name != default_column_family)
        {
            make_table_(std::string(family_descs[i].name),
                        family_handles[i]);
        }
        else
        {
            LOG_INFO("not creating a table for " << family_descs[i].name);
            delete family_handles[i];
        }
    }
}

RocksDataBase::~RocksDataBase()
{
    LOG_INFO("Releasing all tables (" << tables_.size() << ")");

    LOCK();
    tables_.clear();
}

TableInterfacePtr
RocksDataBase::open(const std::string& nspace)
{
    LOCK();

    if (nspace == default_column_family)
    {
        LOG_ERROR("Opening the default column family is not permitted");
        throw RocksDataBaseException("Opening the default column family is not permitted",
                                     nspace.c_str());
    }

    auto it = tables_.find(nspace);
    if (it != tables_.end())
    {
        return it->second;
    }
    else
    {
        rdb::ColumnFamilyHandle* h;

        HANDLE(db_->CreateColumnFamily(rocks_config_.column_family_options(),
                                       nspace,
                                       &h));

        return make_table_(nspace,
                           h);
    }
}

RocksTablePtr
RocksDataBase::make_table_(const std::string& nspace,
                           rdb::ColumnFamilyHandle* h)
{
    LOG_TRACE("creating RocksTable for " << nspace);

    VERIFY(nspace != default_column_family);

    std::unique_ptr<rdb::ColumnFamilyHandle> handle(h);

    auto table(std::make_shared<RocksTable>(nspace,
                                            db_,
                                            std::move(handle),
                                            rocks_config_));

    const auto r(tables_.insert(std::make_pair(nspace,
                                               table)));

    VERIFY(r.second);

    return table;
}

std::vector<std::string>
RocksDataBase::list_namespaces()
{
    LOCK();

    std::vector<std::string> nspaces;
    nspaces.reserve(tables_.size());

    for (const auto& p : tables_)
    {
        if (p.first != default_column_family)
        {
            nspaces.emplace_back(p.first);
        }
    }

    return nspaces;
}

void
RocksDataBase::drop(const std::string& nspace)
{
    LOCK();

    if (nspace == default_column_family)
    {
        LOG_ERROR("Dropping the default column family is not permitted");
        throw RocksDataBaseException("Dropping the default column family is not permitted",
                                     nspace.c_str());
    }

    auto it = tables_.find(nspace);
    if (it != tables_.end())
    {
        it->second->drop();
        tables_.erase(it);
    }
}

}
