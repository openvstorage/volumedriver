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

#include "../metadata-server/RocksDataBase.h"
#include "../metadata-server/RocksTable.h"

#include <algorithm>
#include <map>
#include <set>

#include <boost/algorithm/string.hpp>
#include <boost/bimap.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <youtils/FileUtils.h>
#include <youtils/System.h>
#include <gtest/gtest.h>

namespace metadata_server_test
{

namespace fs = boost::filesystem;
namespace mds = metadata_server;
namespace rdb = rocksdb;
namespace vd = volumedriver;
namespace yt = youtils;

using namespace std::string_literals;

class RocksTest
    : public testing::Test
{
protected:
    RocksTest()
        : root_(yt::FileUtils::temp_path() / "RocksTest"s)
        , path_(root_ / "rocksdb"s)
    {}

    virtual void
    SetUp()
    {
        fs::remove_all(root_);
        fs::create_directories(root_);
    }

    virtual void
    TearDown()
    {
        fs::remove_all(root_);
    }

    void
    test_multi_set_n_get_n_delete(size_t multi)
    {
        ASSERT_LT(0U, multi) << "fix your test";

        const std::string nspace("some-namespace");

        std::map<std::string, std::string> map;
        for (size_t i = 0; i < multi; ++i)
        {
            const std::string s(boost::lexical_cast<std::string>(i));
            map.insert(std::make_pair(s,
                                      boost::to_upper_copy(s)));
        }

        ASSERT_EQ(multi, map.size());

        mds::TableInterface::Keys keys;
        keys.reserve(map.size());

        mds::TableInterface::Records records;
        records.reserve(map.size());

        for (const auto& p : map)
        {
            records.emplace_back(mds::Record(mds::Key(p.first),
                                             mds::Value(p.second)));

            keys.emplace_back(mds::Key(p.first));
        }

        auto verify_absence_of_keys([&]
                                    {
                                        mds::RocksDataBase db(path_);
                                        mds::TableInterfacePtr table(db.open(nspace));

                                        ASSERT_TRUE(table != nullptr);

                                        const mds::TableInterface::MaybeStrings
                                            mvals(table->multiget(keys));

                                        EXPECT_EQ(keys.size(), mvals.size());
                                        for (const auto& m : mvals)
                                        {
                                            EXPECT_TRUE(m == boost::none);
                                        }
                                    });

        verify_absence_of_keys();

        {
            mds::RocksDataBase db(path_);
            mds::TableInterfacePtr table(db.open(nspace));

            ASSERT_TRUE(table != nullptr);

            table->multiset(records,
                            Barrier::F,
                            vd::OwnerTag(0));
        }

        {
            mds::RocksDataBase db(path_);
            mds::TableInterfacePtr table(db.open(nspace));

            ASSERT_TRUE(table != nullptr);

            const mds::TableInterface::MaybeStrings mvals(table->multiget(keys));

            EXPECT_EQ(keys.size(), mvals.size());
            for (size_t i = 0; i < keys.size(); ++i)
            {
                ASSERT_TRUE(mvals[i] != boost::none);
                const std::string s(static_cast<const char*>(keys[i].data),
                                    keys[i].size);
                ASSERT_EQ(*mvals[i], map[s]);
            }
        }

        records.clear();
        records.reserve(map.size());

        for (const auto& k : keys)
        {
            records.emplace_back(mds::Record(k,
                                             mds::None()));
        }

        {
            mds::RocksDataBase db(path_);
            mds::TableInterfacePtr table(db.open(nspace));

            ASSERT_TRUE(table != nullptr);

            table->multiset(records,
                            Barrier::F,
                            vd::OwnerTag(0));
        }

        verify_absence_of_keys();
    }

    void
    set(mds::TableInterfacePtr& table,
        const mds::Record& rec,
        Barrier barrier = Barrier::F)
    {
        const mds::TableInterface::Records recs{ rec };
        table->multiset(recs,
                        barrier,
                        vd::OwnerTag(0));
    }

    boost::optional<std::string>
    get(mds::TableInterfacePtr& table,
        const mds::Key& key)
    {
        const mds::TableInterface::Keys keys{ key };
        return table->multiget(keys)[0];
    }

    const fs::path root_;
    const fs::path path_;
};

TEST_F(RocksTest, construction)
{
    mds::RocksDataBase db(path_);
}

// cf. OVS-1605
TEST_F(RocksTest, construction_in_existing_directory)
{
    fs::create_directories(path_);
    mds::RocksDataBase db(path_);
}

TEST_F(RocksTest, construction_in_existing_non_empty_directory)
{
    fs::create_directories(path_);
    const fs::path f(path_ / ".hidden");

    yt::FileUtils::touch(f);
    ASSERT_TRUE(fs::exists(f));

    ASSERT_THROW(mds::RocksDataBase db(path_),
                 std::exception);

    ASSERT_TRUE(fs::exists(f));
}

TEST_F(RocksTest, construction_with_path_that_is_not_a_directory)
{
    const fs::path f(path_);

    yt::FileUtils::touch(f);
    ASSERT_TRUE(fs::exists(f));
    ASSERT_TRUE(fs::is_regular_file(f));

    ASSERT_THROW(mds::RocksDataBase db(path_),
                 std::exception);

    ASSERT_TRUE(fs::exists(f));
}

TEST_F(RocksTest, hidden_default_table)
{
    mds::RocksDataBase db(path_);
    EXPECT_TRUE(db.list_namespaces().empty());

    EXPECT_THROW(db.open(rdb::kDefaultColumnFamilyName),
                 std::exception);

    EXPECT_TRUE(db.list_namespaces().empty());

    EXPECT_THROW(db.drop(rdb::kDefaultColumnFamilyName),
                 std::exception);
}

TEST_F(RocksTest, tables)
{
    auto check_nspaces([&](mds::RocksDataBase& db,
                           const std::set<std::string>& nspaces)
                       {
                           const std::vector<std::string> nspacev(db.list_namespaces());

                           EXPECT_EQ(nspaces.size(),
                                     nspacev.size());

                           std::set<std::string> nspaces2(nspaces);

                           for (const auto& n : nspacev)
                           {
                               EXPECT_EQ(1U, nspaces2.erase(n));
                           }
                       });

    std::set<std::string> nspaces;

    {
        mds::RocksDataBase db(path_);

        check_nspaces(db, nspaces);

        for (const auto& n : { "foo"s,
                               "bar"s,
                               "baz"s })
        {
            db.open(n);
            ASSERT_TRUE(nspaces.insert(n).second);
        }

        check_nspaces(db, nspaces);

        auto it = nspaces.begin();
        ASSERT_TRUE(it != nspaces.end());

        db.drop(*it);
        nspaces.erase(it);

        check_nspaces(db, nspaces);
    }

    {
        mds::RocksDataBase db(path_);
        check_nspaces(db, nspaces);
    }
}

TEST_F(RocksTest, set_n_get_n_delete)
{
    test_multi_set_n_get_n_delete(1);
}

TEST_F(RocksTest, multi_set_n_get)
{
    test_multi_set_n_get_n_delete(64);
}

TEST_F(RocksTest, cleared_table)
{
    const auto pair1(std::make_pair("one"s,
                                    "een"s));

    const mds::Record rec1(mds::Key(pair1.first),
                           mds::Value(pair1.second));

    const auto pair2(std::make_pair("two"s, "twee"s));

    const mds::Record rec2(mds::Key(pair2.first),
                           mds::Value(pair2.second));

    const std::string nspace("some-namespace");

    {
        mds::RocksDataBase db(path_);
        mds::TableInterfacePtr table(db.open(nspace));

        set(table, rec1);

        const auto maybe_str(get(table, rec1.key));
        ASSERT_TRUE(maybe_str != boost::none);
        ASSERT_EQ(pair1.second,
                  *maybe_str);
    }

    {
        mds::RocksDataBase db(path_);
        mds::TableInterfacePtr table(db.open(nspace));

        table->clear(vd::OwnerTag(0));

        const auto maybe_str(get(table, rec1.key));
        ASSERT_TRUE(maybe_str == boost::none);

        set(table, rec2);
    }

    {
        mds::RocksDataBase db(path_);
        mds::TableInterfacePtr table(db.open(nspace));

        const auto maybe_str(get(table, rec2.key));
        ASSERT_TRUE(maybe_str != boost::none);
        ASSERT_EQ(pair2.second,
                  *maybe_str);
    }
}

TEST_F(RocksTest, dropped_table)
{
    const auto pair(std::make_pair("key"s,
                                   "val"s));

    const std::string nspace("some-namespace");

    mds::RocksDataBase db(path_);

    const mds::TableInterface::Keys keys{ mds::Key(pair.first) };

    auto verify_absence([&](mds::TableInterfacePtr& table)
                        {
                            ASSERT_TRUE(table != nullptr);

                            const mds::TableInterface::MaybeStrings
                                ms(table->multiget(keys));
                            ASSERT_EQ(keys.size(), ms.size());
                            ASSERT_TRUE(ms[0] == boost::none);
                        });

    auto verify_presence([&](mds::TableInterfacePtr& table)
                         {
                             ASSERT_TRUE(table != nullptr);

                             const mds::TableInterface::MaybeStrings
                                 ms(table->multiget(keys));
                             ASSERT_EQ(keys.size(), ms.size());
                             ASSERT_TRUE(ms[0] != boost::none);
                             ASSERT_EQ(pair.second, *ms[0]);
                         });

    {
        mds::TableInterfacePtr table(db.open(nspace));

        verify_absence(table);

        const mds::TableInterface::Records records{ mds::Record(mds::Key(pair.first),
                                                                mds::Value(pair.second)) };
        table->multiset(records,
                        Barrier::F,
                        vd::OwnerTag(0));

        verify_presence(table);

        db.drop(nspace);

        const std::vector<std::string> nspaces(db.list_namespaces());

        EXPECT_TRUE(std::find(nspaces.begin(),
                              nspaces.end(),
                              nspace) == nspaces.end());

        verify_presence(table);
    }

    mds::TableInterfacePtr table(db.open(nspace));

    verify_absence(table);
}

// Our RocksDataBase runs without a WAL - a barrier is to make sure that
// everything *before* it is flushed from the memtable.
// See if that works as expected, i.e. data is flushed out of the memtable.
// Disabled as the test needs to be run twice with ROCKSTEST_ROOT set to
// something else than the default: the first run will add entries and kill
// then itself, the second one tries to get the written entries back.
TEST_F(RocksTest, DISABLED_barrier)
{
    const fs::path p(yt::System::get_env_with_default("ROCKSTEST_ROOT",
                                                      path_.string()));
    fs::create_directories(p);

    mds::RocksDataBase db(p);

    const std::string nspace("some-namespace");

    mds::TableInterfacePtr table(db.open(nspace));

    const std::string key1("some-key");
    const std::string val1("some-val");

    const std::string key2("barrier-key");
    const std::string val2("barrier-val");

    auto maybe_str(get(table,
                       key1));
    if (not maybe_str)
    {
        set(table,
            mds::Record(mds::Key(key1),
                        mds::Value(val1)),
            Barrier::F);

        set(table,
            mds::Record(mds::Key(key2),
                        mds::Value(val2)),
            Barrier::T);

        ::kill(::getpid(),
               SIGKILL);
    }

    EXPECT_EQ(val1,
              *maybe_str);
}

}
