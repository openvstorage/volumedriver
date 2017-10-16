// Copyright (C) 2017 iNuron NV
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

#include "RocksDataBaseClient.h"

#include <rocksdb/metadata.h>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/python/class.hpp>
#include <boost/python/data_members.hpp>
#include <boost/python/enum.hpp>
#include <boost/python/return_value_policy.hpp>

#include <youtils/Logging.h>
#include<youtils/python/IterableConverter.h>
#include<youtils/python/OptionalConverter.h>

#include <volumedriver/metadata-server/RocksConfig.h>
#include <volumedriver/metadata-server/RocksDataBase.h>

namespace volumedriver
{

namespace python
{

namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace mds = metadata_server;
namespace vd = volumedriver;
namespace yt = youtils;

using namespace std::literals::string_literals;

namespace
{

class DataBaseAdapter;

// XXX: (our?) boost::python doesn't support std::shared_ptr. Look into fixing
// this instead of creating wrappers / adapters like this one.
class TableAdapter
{
public:
    ~TableAdapter() = default;

    TableAdapter(const TableAdapter&) = default;

    TableAdapter&
    operator=(const TableAdapter&) = default;

    rocksdb::ColumnFamilyMetaData
    column_family_metadata()
    {
        return table->column_family_metadata();
    }

    void
    compact(bool reduce_level,
            int target_level)
    {
        table->compact(reduce_level,
                       target_level);
    }

    std::string
    repr() const
    {
        return "MDSRocksTableHandle("s + table->nspace() + ")"s;
    }

    boost::optional<std::string>
    get_property(const std::string& prop)
    {
        return table->get_property(prop);
    }

    mds::TableInterface::MaybeStrings
    multiget(const std::vector<std::string>& keys)
    {
        mds::TableInterface::Keys ks;
        ks.reserve(keys.size());

        for (const auto& k : keys)
        {
            ks.emplace_back(k);
        }

        return table->multiget(ks);
    }

private:
    DECLARE_LOGGER("RocksTableHandle");

    friend class DataBaseAdapter;

    mds::RocksTablePtr table;

    explicit TableAdapter(const mds::RocksTablePtr& t)
        : table(t)
    {
        VERIFY(table != nullptr);
    }
};

class DataBaseAdapter
{
public:
    DataBaseAdapter(const fs::path& p,
                    const mds::RocksConfig& cfg)
        : db(p,
             cfg)
        , path(p)
    {}

    ~DataBaseAdapter() = default;

    DataBaseAdapter(const DataBaseAdapter&) = delete;

    DataBaseAdapter&
    operator=(const DataBaseAdapter&) = delete;

    std::string
    repr() const
    {
        return "MDSRocksDataBaseClient(path="s + path.string() + ")"s;
    }

    boost::optional<TableAdapter>
    open(const std::string& nspace)
    {
        mds::RocksTablePtr t(db.open(nspace,
                                     CreateIfNecessary::F));
        if (t)
        {
            return TableAdapter(t);
        }
        else
        {
            return boost::none;
        }
    }

    std::vector<std::string>
    list_namespaces()
    {
        return db.list_namespaces();
    }

    void
    drop(const std::string& nspace)
    {
        db.drop(nspace);
    }

private:
    mds::RocksDataBase db;
    fs::path path;
};

}

void
RocksDataBaseClient::registerize()
{
#define MAKE_GETTER(typ, name)                                          \
    add_property(#name,                                                 \
                 bpy::make_getter(&typ::name,                           \
                                   bpy::return_value_policy<bpy::return_by_value>()))

#define MAKE_GETTER_SETTER(typ, name)                                   \
    add_property(#name,                                                 \
                 bpy::make_getter(&typ::name,                           \
                                  bpy::return_value_policy<bpy::return_by_value>()), \
                 bpy::make_setter(&typ::name))


    bpy::class_<rocksdb::SstFileMetaData>("RocksSstFileMetaData",
                                          "RocksDB SST file metadata",
                                          bpy::no_init)
        .MAKE_GETTER(rocksdb::SstFileMetaData, size)
        .MAKE_GETTER(rocksdb::SstFileMetaData, name)
        .MAKE_GETTER(rocksdb::SstFileMetaData, db_path)
        .MAKE_GETTER(rocksdb::SstFileMetaData, smallest_seqno)
        .MAKE_GETTER(rocksdb::SstFileMetaData, largest_seqno)
        .MAKE_GETTER(rocksdb::SstFileMetaData, smallestkey)
        .MAKE_GETTER(rocksdb::SstFileMetaData, largestkey)
        .MAKE_GETTER(rocksdb::SstFileMetaData, being_compacted)
        ;

    REGISTER_ITERABLE_CONVERTER(std::vector<rocksdb::SstFileMetaData>);

    bpy::class_<rocksdb::LevelMetaData>("RocksLevelMetaData",
                                        "RocksDB level metadata",
                                        bpy::no_init)
        .MAKE_GETTER(rocksdb::LevelMetaData, level)
        .MAKE_GETTER(rocksdb::LevelMetaData, size)
        .MAKE_GETTER(rocksdb::LevelMetaData, files)
        ;

    REGISTER_ITERABLE_CONVERTER(std::vector<rocksdb::LevelMetaData>);

    bpy::class_<rocksdb::ColumnFamilyMetaData>("RocksColumnFamilyMetaData",
                                               "RocksDB column family metadata",
                                               bpy::no_init)
        .MAKE_GETTER(rocksdb::ColumnFamilyMetaData, size)
        .MAKE_GETTER(rocksdb::ColumnFamilyMetaData, file_count)
        .MAKE_GETTER(rocksdb::ColumnFamilyMetaData, name)
        .MAKE_GETTER(rocksdb::ColumnFamilyMetaData, levels)
        ;

    bpy::enum_<mds::RocksConfig::CompactionStyle>("MDSRocksConfigCompactionStyle")
        .value("Level", mds::RocksConfig::CompactionStyle::Level)
        .value("Universal", mds::RocksConfig::CompactionStyle::Universal)
        .value("Fifo", mds::RocksConfig::CompactionStyle::Fifo)
        .value("None", mds::RocksConfig::CompactionStyle::None)
        ;

    REGISTER_OPTIONAL_CONVERTER(mds::RocksConfig::CompactionStyle);

    // we're not exposing all members for now - some only make sense when
    // using the data base (read / write cache settings etc).
    bpy::class_<mds::RocksConfig>("MDSRocksConfig",
                                  "MDS RocksDB config",
                                  bpy::init<>())
        .MAKE_GETTER_SETTER(mds::RocksConfig, compaction_style)
        .MAKE_GETTER_SETTER(mds::RocksConfig, num_levels)
        .MAKE_GETTER_SETTER(mds::RocksConfig, target_file_size_base)
        .MAKE_GETTER_SETTER(mds::RocksConfig, max_bytes_for_level_base)
        ;

    REGISTER_ITERABLE_CONVERTER(mds::TableInterface::MaybeStrings);

    bpy::class_<TableAdapter>("MDSRocksTableHandle",
                              "MDS RocksDB table handle",
                              bpy::no_init)
        .def("__repr__",
             &TableAdapter::repr)
        .def("column_family_metadata",
             &TableAdapter::column_family_metadata,
             (bpy::args("nspace")),
             "get the RocksDB column family metadata for a given namespace\n"
             "@param nspace: string, namespace name\n"
             "@returns: RocksColumnFamilyMetaData\n")
        .def("compact",
             &TableAdapter::compact,
             (bpy::args("reduce_level") = false,
              bpy::args("target_level") = -1),
             "run compaction of all keys of the column family\n"
             "@param reduce_level: bool, cf. rocksdb::DB::CompactRange\n"
             "@param target_level: int, cf. rocksdb::DB::CompactRange\n")
        .def("get_property",
             &TableAdapter::get_property,
             (bpy::args("property")),
             "get a RocksDB property (cf. rocksdb::DB::GetProperty)\n"
             "@param property: string, property key cf. rocksdb::DB::GetProperty\n"
             "@returns: optional string if property is supported\n")
        .def("multiget",
             &TableAdapter::multiget,
             (bpy::args("keys")),
             "perform a multiget\n"
             "@param keys: [string], list of keys to get\n"
             "@returns: list of optional strings\n")
        ;

    REGISTER_OPTIONAL_CONVERTER(TableAdapter);

    bpy::class_<DataBaseAdapter,
                boost::noncopyable>("MDSRocksDataBaseClient",
                                    "MDS RocksDB database client",
                                    bpy::init<const fs::path&,
                                              const mds::RocksConfig&>((bpy::args("path"),
                                                                        bpy::args("rocks_config") = mds::RocksConfig()),
                                                                       "create an MDS RocksDB client\n"
                                                                       "@param path: string, path to RocksDB\n"
                                                                       "@param rocks_config: MDSRocksConfig\n"))
        .def("__repr__",
             &DataBaseAdapter::repr)
        .def("drop",
             &DataBaseAdapter::drop,
             (bpy::args("nspace")),
             "drop the MDS namespace from the database\n"
             "@param nspace: string, namespace\n")
        .def("list_namespaces",
             &DataBaseAdapter::list_namespaces,
             "retrieve MDS namespaces\n"
             "@returns: [string], list of MDS namespaces\n")
        .def("open",
             &DataBaseAdapter::open,
             (bpy::args("nspace")),
             "open an MDS namespace\n"
             "@param nspace: string, namespace\n"
             "@returns: MDSRocksTableHandle\n")
        ;
}

}

}
