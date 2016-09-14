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

#include "MDSTestSetup.h"

#include <boost/algorithm/string.hpp>

#include <youtils/FileUtils.h>
#include <gtest/gtest.h>
#include <youtils/System.h>
#include <youtils/wall_timer.h>

#include <backend/BackendTestSetup.h>

#include "../MDSMetaDataBackend.h"
#include "../MDSNodeConfig.h"
#include "../MetaDataBackendConfig.h"
#include "../metadata-server/ClientNG.h"
#include "../metadata-server/ServerNG.h"
#include "../metadata-server/Manager.h"
#include "../metadata-server/PythonClient.h"
#include "../metadata-server/Utils.h"

namespace volumedrivertest
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace mds = metadata_server;
namespace vd = volumedriver;
namespace yt = youtils;
namespace ytt = youtilstest;

using namespace std::string_literals;

namespace
{

size_t shmem_size = yt::System::get_env_with_default("MDS_TEST_SHMEM_SIZE",
                                                     8ULL << 10);

struct Config
{
    Config(ForceRemote f,
           size_t s)
        : force_remote(f)
        , shmem_size(s)
    {}

    ForceRemote force_remote;
    size_t shmem_size;
};

}

class MetaDataServerTest
    : public ::testing::TestWithParam<Config>
    , public be::BackendTestSetup
{
    DECLARE_LOGGER("MetaDataServerTest");

public:
    MetaDataServerTest()
        : be::BackendTestSetup()
        , root_(yt::FileUtils::temp_path() / "MetaDataServerTest")
    {
        fs::remove_all(root_);
        fs::create_directories(root_);
        initialize_connection_manager();

        mds_test_setup_ = std::make_unique<MDSTestSetup>(root_ / "mds");
        mds_manager_ = mds_test_setup_->make_manager(cm_,
                                                     1);
    }

    virtual ~MetaDataServerTest()
    {
        mds_manager_.reset();
        mds_test_setup_.reset();
        uninitialize_connection_manager();

        fs::remove_all(root_);
    }

    vd::MDSNodeConfig
    mds_config()
    {
        const mds::ServerConfigs cfgs(mds_manager_->server_configs());
        EXPECT_EQ(1U, cfgs.size());
        return cfgs[0].node_config;
    }

    mds::ClientNG::Ptr
    make_client(const boost::optional<std::chrono::seconds>& timeout = boost::none)
    {
        return mds::ClientNG::create(mds_config(),
                                     GetParam().shmem_size,
                                     timeout,
                                     GetParam().force_remote);
    }

    // largely duplicates the method of the same name in RocksTest.cpp - try to unify?
    void
    test_multi_set_n_get_n_delete(size_t multi)
    {
        ASSERT_LT(0U, multi) << "fix your test";

        be::BackendTestSetup::WithRandomNamespace wrns("",
                                                       cm_);

        const std::string nspace(wrns.ns().str());

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
                                        auto client(make_client());
                                        mds::TableInterfacePtr table(client->open(nspace));

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

        const vd::OwnerTag owner_tag(1);

        {
            auto client(make_client());
            mds::TableInterfacePtr table(client->open(nspace));

            ASSERT_TRUE(table != nullptr);
            table->set_role(mds::Role::Master,
                            owner_tag);

            table->multiset(records,
                            Barrier::F,
                            owner_tag);
        }

        {
            auto client(make_client());
            mds::TableInterfacePtr table(client->open(nspace));

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
            auto client(make_client());
            mds::TableInterfacePtr table(client->open(nspace));

            ASSERT_TRUE(table != nullptr);

            table->multiset(records,
                            Barrier::F,
                            vd::OwnerTag(1));
        }

        verify_absence_of_keys();
    }

    void
    set(mds::TableInterfacePtr& table,
        const mds::Record& rec,
        const boost::optional<vd::OwnerTag>& owner_tag = boost::none)
    {
        const mds::TableInterface::Records recs{ rec };
        const vd::OwnerTag tag = owner_tag ? *owner_tag : table->owner_tag();

        table->multiset(recs,
                        Barrier::F,
                        tag);
    }

    boost::optional<std::string>
    get(mds::TableInterfacePtr& table,
        const mds::Key& key)
    {
        const mds::TableInterface::Keys keys{ key };
        return table->multiget(keys)[0];
    }

    void
    test_multiget_performance(bool empty,
                              bool ipc = true)
    {
        const size_t iterations =
            yt::System::get_env_with_default<size_t>("MDS_TEST_ITERATIONS",
                                                     10000);
        const size_t batch_size =
            yt::System::get_env_with_default<size_t>("MDS_TEST_BATCH_SIZE",
                                                     1);

        const size_t vsize =
            yt::System::get_env_with_default<size_t>("MDS_TEST_VALUE_SIZE",
                                                     4096);

        const size_t nclients =
            yt::System::get_env_with_default<size_t>("MDS_TEST_CLIENTS",
                                                     1);

        ASSERT_LT(0U, iterations) << "fix your test";
        ASSERT_LT(0U, batch_size) << "fix your test";
        ASSERT_LT(0U, vsize) << "fix your test";
        ASSERT_LT(0U, nclients) << "fix your test";

        const vd::OwnerTag owner_tag(1);

        auto fun([&]() -> double
                 {
                     be::BackendTestSetup::WithRandomNamespace wrns("",
                                                                    cm_);
                     const std::string nspace(wrns.ns().str());

                     mds::TableInterfacePtr table;
                     if (ipc)
                     {
                         auto client(make_client());
                         table = client->open(nspace);
                     }
                     else
                     {
                         mds::DataBaseInterfacePtr
                             db(mds_manager_->find(mds_config()));
                         table = db->open(nspace);
                     }

                     table->set_role(mds::Role::Master,
                                     owner_tag);

                     std::vector<uint64_t> ks;
                     std::vector<std::string> vs;
                     mds::TableInterface::Keys keys;
                     mds::TableInterface::Records recs;

                     ks.reserve(batch_size);
                     vs.reserve(batch_size);
                     keys.reserve(batch_size);
                     recs.reserve(batch_size);

                     for (size_t i = 0; i < batch_size; ++i)
                     {
                         const uint64_t k = i;
                         auto v(boost::lexical_cast<std::string>(k));
                         v.resize(vsize);
                         ks.push_back(k);
                         vs.emplace_back(std::move(v));
                         keys.emplace_back(mds::Key(ks.back()));
                         recs.emplace_back(mds::Record(mds::Key(ks.back()),
                                                       mds::Value(vs.back())));
                     }

                     if (not empty)
                     {
                         table->multiset(recs,
                                         Barrier::F,
                                         owner_tag);
                     }

                     yt::wall_timer w;

                     for (size_t i = 0; i < iterations; ++i)
                     {
                         const mds::TableInterface::MaybeStrings mvals(table->multiget(keys));

                         EXPECT_EQ(batch_size, mvals.size());
                         for (const auto& mval : mvals)
                         {
                             if (empty)
                             {
                                 EXPECT_EQ(boost::none,
                                           mval);
                             }
                             else
                             {
                                 EXPECT_EQ(vsize, mval->size());
                             }
                         }
                     }

                     return iterations / w.elapsed();
                 });

        std::vector<std::future<double>> iopsv;
        iopsv.reserve(nclients);

        for (size_t i = 0; i < nclients; ++i)
        {
            iopsv.emplace_back(std::async(std::launch::async,
                                          fun));
        }

        double iops = 0;
        for (auto& f : iopsv)
        {
            iops += f.get();
        }

        std::cout << iterations << " multiget(" << batch_size <<
            ") iterations (clients " << nclients <<
            ", shmem size " << GetParam().shmem_size <<
            ", value size " << vsize << ") -> " <<
            iops << " IOPS" << std::endl;
    }

protected:
    const fs::path root_;
    std::unique_ptr<MDSTestSetup> mds_test_setup_;
    std::unique_ptr<mds::Manager> mds_manager_;
};

TEST_P(MetaDataServerTest, no_updates_on_slave)
{
    auto client(make_client());

    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    const std::string nspace(wrns.ns().str());

    mds::TableInterfacePtr table(client->open(nspace));

    const auto pair(std::make_pair("key"s,
                                   "val"s));

    const mds::Record record(mds::Key(pair.first),
                             mds::Value(pair.second));

    const vd::OwnerTag owner_tag(1);

    EXPECT_THROW(set(table, record, owner_tag),
                 std::exception);

    EXPECT_TRUE(get(table, record.key) == boost::none);

    table->set_role(mds::Role::Master,
                    owner_tag);

    set(table, record, owner_tag);

    const auto maybe_string(get(table, record.key));
    ASSERT_TRUE(maybe_string != boost::none);
    EXPECT_EQ(pair.second, *maybe_string);
}

TEST_P(MetaDataServerTest, tables)
{
    auto check_nspaces([&](mds::ClientNG& client,
                           const std::set<std::string>& nspaces)
                       {
                           const std::vector<std::string> nspacev(client.list_namespaces());

                           EXPECT_EQ(nspaces.size(),
                                     nspacev.size());

                           std::set<std::string> nspaces2(nspaces);

                           for (const auto& n : nspacev)
                           {
                               EXPECT_EQ(1U, nspaces2.erase(n));
                           }
                       });

    std::set<std::string> nspaces;

    be::BackendTestSetup::WithRandomNamespace wrns1("",
                                                    cm_);
    be::BackendTestSetup::WithRandomNamespace wrns2("",
                                                    cm_);
    be::BackendTestSetup::WithRandomNamespace wrns3("",
                                                    cm_);

    {
        auto client(make_client());
        check_nspaces(*client,
                      nspaces);

        for (const auto& n : { wrns1.ns().str(),
                               wrns2.ns().str(),
                               wrns3.ns().str() })
        {
            client->open(n);
            ASSERT_TRUE(nspaces.insert(n).second);
        }

        check_nspaces(*client, nspaces);

        auto it = nspaces.begin();
        ASSERT_TRUE(it != nspaces.end());

        client->drop(*it);
        nspaces.erase(it);

        check_nspaces(*client, nspaces);
    }

    {
        auto client(make_client());
        check_nspaces(*client, nspaces);
    }
}

TEST_P(MetaDataServerTest, set_n_get_n_delete)
{
    test_multi_set_n_get_n_delete(1);
}

TEST_P(MetaDataServerTest, multi_set_n_get)
{
    test_multi_set_n_get_n_delete(64);
}

TEST_P(MetaDataServerTest, cleared_table)
{
    const auto pair1(std::make_pair("one"s,
                                    "een"s));

    const mds::Record rec1(mds::Key(pair1.first),
                           mds::Value(pair1.second));

    const auto pair2(std::make_pair("two"s, "twee"s));

    const mds::Record rec2(mds::Key(pair2.first),
                           mds::Value(pair2.second));

    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);
    const std::string nspace(wrns.ns().str());

    const vd::OwnerTag owner_tag(1);

    {
        auto client(make_client());
        mds::TableInterfacePtr table(client->open(nspace));
        table->set_role(mds::Role::Master,
                        owner_tag);

        set(table, rec1);

        const auto maybe_str(get(table, rec1.key));
        ASSERT_TRUE(maybe_str != boost::none);
        ASSERT_EQ(pair1.second,
                  *maybe_str);
    }

    {
        auto client(make_client());
        mds::TableInterfacePtr table(client->open(nspace));

        table->clear(owner_tag);

        const auto maybe_str(get(table, rec1.key));
        ASSERT_TRUE(maybe_str == boost::none);

        set(table, rec2, owner_tag);
    }

    {
        auto client(make_client());
        mds::TableInterfacePtr table(client->open(nspace));

        const auto maybe_str(get(table, rec2.key));
        ASSERT_TRUE(maybe_str != boost::none);
        ASSERT_EQ(pair2.second,
                  *maybe_str);
    }
}

TEST_P(MetaDataServerTest, python_client_exceptional)
{
    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    const std::string nspace(wrns.ns().str());

    mds::PythonClient client(mds_config(),
                             GetParam().force_remote);

    EXPECT_TRUE(client.list_namespaces().empty());

    EXPECT_THROW(client.remove_namespace(nspace),
                 std::exception);

    EXPECT_THROW(client.get_role(nspace),
                 std::exception);

    const vd::OwnerTag owner_tag(1);

    EXPECT_THROW(client.set_role(nspace,
                                 mds::Role::Master,
                                 owner_tag),
                 std::exception);

    EXPECT_THROW(client.set_role(nspace,
                                 mds::Role::Slave,
                                 owner_tag),
                 std::exception);

    EXPECT_THROW(client.get_cork_id(nspace),
                 std::exception);
}

TEST_P(MetaDataServerTest, python_client_role)
{
    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    const std::string nspace(wrns.ns().str());

    mds::PythonClient client(mds_config(),
                             GetParam().force_remote);

    EXPECT_TRUE(client.list_namespaces().empty());

    client.create_namespace(nspace);

    ASSERT_EQ(1U,
              client.list_namespaces().size());

    ASSERT_EQ(nspace,
              client.list_namespaces().front());

    ASSERT_EQ(mds::Role::Slave,
              client.get_role(nspace));

    const vd::OwnerTag owner_tag(1);

    client.set_role(nspace,
                    mds::Role::Master,
                    owner_tag);

    ASSERT_EQ(mds::Role::Master,
              client.get_role(nspace));

    client.create_namespace(nspace);

    ASSERT_EQ(mds::Role::Master,
              client.get_role(nspace));
}

TEST_P(MetaDataServerTest, get_owner_tag)
{
    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    auto client(make_client());
    mds::TableInterfacePtr table(client->open(wrns.ns().str()));

    EXPECT_EQ(vd::OwnerTag(0),
              table->owner_tag());

    const vd::OwnerTag owner_tag(1234);
    table->set_role(mds::Role::Master,
                    owner_tag);

    EXPECT_EQ(owner_tag,
              table->owner_tag());
}

TEST_P(MetaDataServerTest, owner_tag)
{
    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    auto client(make_client());
    mds::TableInterfacePtr table(client->open(wrns.ns().str()));

    const vd::OwnerTag owner_tag(2);

    table->set_role(mds::Role::Master,
                    owner_tag);

    const mds::Key key("key"s);
    const std::string val1("val1");

    set(table,
        mds::Record(key,
                    mds::Value(val1)),
        owner_tag);

    const auto check([&]
                     {
                         const boost::optional<std::string> maybe_string(get(table,
                                                                             key));

                         ASSERT_NE(boost::none,
                                   maybe_string);
                         ASSERT_EQ(val1,
                                   *maybe_string);
                     });

    check();

    const vd::OwnerTag old_owner_tag(1);
    ASSERT_NE(owner_tag,
              old_owner_tag);

    const std::string val2("val2");

    EXPECT_THROW(set(table,
                     mds::Record(key,
                                 mds::Value(val2)),
                     old_owner_tag),
                 std::exception);

    EXPECT_THROW(table->clear(old_owner_tag),
                 std::exception);

    check();
}

TEST_P(MetaDataServerTest, python_client_cork_id)
{
    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    const std::string nspace(wrns.ns().str());

    mds::PythonClient client(mds_config(),
                             GetParam().force_remote);

    EXPECT_TRUE(client.list_namespaces().empty());

    client.create_namespace(nspace);

    EXPECT_EQ(boost::none,
              client.get_cork_id(nspace));

    const yt::UUID cork;

    const vd::OwnerTag owner_tag(1);

    // no updates while slave
    client.set_role(nspace,
                    mds::Role::Master,
                    owner_tag);

    {
        vd::MDSMetaDataBackend backend(mds_config(),
                                       wrns.ns(),
                                       owner_tag,
                                       boost::none);
        backend.setCork(cork);
    }

    auto maybe_cork_str(client.get_cork_id(nspace));

    ASSERT_TRUE(maybe_cork_str != boost::none);
    ASSERT_EQ(cork.str(),
              *maybe_cork_str);
}

TEST_P(MetaDataServerTest, ping)
{
    const size_t size =
        yt::System::get_env_with_default<size_t>("MDS_TEST_VALUE_SIZE",
                                                 4096);
    const size_t count =
        yt::System::get_env_with_default<size_t>("MDS_TEST_ITERATIONS",
                                                 10000);

    const std::vector<uint8_t> wbuf(size, 42);
    std::vector<uint8_t> rbuf(size);

    auto client(make_client());

    yt::wall_timer w;

    for (uint32_t i = 0; i < count; ++i)
    {
        client->ping(wbuf,
                     rbuf);
    }

    const double t = w.elapsed();

    std::cout << count << " iterations with messages of " << size <<
        " bytes (shmem size: " << GetParam().shmem_size << ") took " << t <<
        " seconds -> " << count / t << " IOPS" << std::endl;

    EXPECT_EQ(0,
              memcmp(wbuf.data(),
                     rbuf.data(),
                     size));
}

TEST_P(MetaDataServerTest, exceeding_the_shmem_size)
{
    const size_t limit = GetParam().shmem_size + 1;
    const bool expect_overrun =
        GetParam().force_remote == ForceRemote::F and
        GetParam().shmem_size > 0;

    std::vector<std::string> vec;

    mds::TableInterface::Keys keys;
    mds::TableInterface::Records recs;

    size_t size = 0;

    while (size < limit)
    {
        const std::string key(boost::lexical_cast<std::string>(size));
        vec.push_back(key);
        size += key.size();
    }

    for (const auto& k : vec)
    {
        keys.emplace_back(mds::Key(k));
        recs.emplace_back(mds::Record(mds::Key(k),
                                      mds::Value(k)));
    }

    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    auto client(make_client());
    auto table(client->open(wrns.ns().str()));

    const vd::OwnerTag owner_tag(1);

    table->set_role(mds::Role::Master,
                    owner_tag);

    auto shmem_out_overruns([&]() -> uint64_t
                            {
                                mds::ClientNG::OutCounters out;
                                mds::ClientNG::InCounters in;
                                client->counters(out,
                                                 in);

                                return out.shmem_overruns;
                            });

    auto shmem_in_overruns([&]() -> uint64_t
                            {
                                mds::ClientNG::OutCounters out;
                                mds::ClientNG::InCounters in;
                                client->counters(out,
                                                 in);

                                return in.shmem_overruns;
                            });

    EXPECT_EQ(0U, shmem_out_overruns());
    EXPECT_EQ(0U, shmem_in_overruns());

    table->multiset(recs,
                    Barrier::F,
                    owner_tag);

    EXPECT_EQ(expect_overrun ? 1U : 0U, shmem_out_overruns());
    EXPECT_EQ(0U, shmem_in_overruns());

    mds::TableInterface::MaybeStrings mvals(table->multiget(keys));

    EXPECT_EQ(expect_overrun ? 2U : 0U, shmem_out_overruns());
    EXPECT_EQ(expect_overrun ? 1U : 0U, shmem_in_overruns());

    ASSERT_EQ(keys.size(),
              mvals.size());

    for (size_t i = 0; i < mvals.size(); ++i)
    {
        ASSERT_TRUE(mvals[i] != boost::none);
        ASSERT_EQ(vec[i], *mvals[i]);
    }
}

TEST_P(MetaDataServerTest, empty_multiget_performance)
{
    test_multiget_performance(true,
                              true);
}

TEST_P(MetaDataServerTest, multiget_performance)
{
    test_multiget_performance(false,
                              true);
}

TEST_P(MetaDataServerTest, empty_multiget_performance_local)
{
    test_multiget_performance(true,
                              false);
}

TEST_P(MetaDataServerTest, multiget_performance_local)
{
    test_multiget_performance(false,
                              false);
}

INSTANTIATE_TEST_CASE_P(MetaDataServerTests,
                        MetaDataServerTest,
                        ::testing::Values(Config(ForceRemote::F,
                                                 0),
                                          Config(ForceRemote::F,
                                                 shmem_size),
                                          Config(ForceRemote::T,
                                                 0)));
}
