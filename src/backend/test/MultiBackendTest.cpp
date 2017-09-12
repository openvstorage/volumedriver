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

#include "BackendTestBase.h"

#include "../ConnectionPool.h"
#include "../MultiConfig.h"
#include "../NamespacePoolSelector.h"
#include "../RoundRobinPoolSelector.h"

#include <set>

#include <boost/chrono.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/thread/thread.hpp>

#include <youtils/Chooser.h>
#include <youtils/FileUtils.h>
#include <youtils/System.h>
#include <youtils/UUID.h>

namespace backendtest
{

using namespace backend;
using youtils::Chooser;

namespace bc = boost::chrono;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace yt = youtils;

class MultiBackendTest
    : public BackendTestBase
{
public:
    MultiBackendTest()
        : BackendTestBase("multibackendtest")
        , multi_dir_base_(yt::FileUtils::temp_path("multi_dir_base"))
        , stop_switcher(false)
    {

        for(size_t i = 0; i < num_backup_backends_; ++i)
        {
            std::stringstream ss;
            ss << "multi_dir_" << i;
            multi_dirs_.push_back(yt::FileUtils::temp_path(ss.str()));
        }
    }

    void
    SetUp()
    {
        if(fs::exists(multi_dir_base_))
        {
            fs::remove_all(multi_dir_base_);
        }
        for(size_t i = 0; i < num_backup_backends_; ++i)
        {
            destroy_multi_dir(i);
        }

        fs::create_directories(multi_dir_base_);
    }

    void
    TearDown()
    {
        if(fs::exists(multi_dir_base_))
        {
            fs::remove_all(multi_dir_base_);
        }
        for(size_t i = 0; i < num_backup_backends_; ++i)
        {
            destroy_multi_dir(i);
        }

    }

    void
    setup_multi_dir(size_t i)
    {
        ASSERT_LT(i, num_backup_backends_);

        fs::path& dir = multi_dirs_[i];

        if(not fs::exists(dir))
        {
            fs::create_symlink(multi_dir_base_, dir);
        }
    }

    void
    setup_multi_dirs(size_t num_backends = num_backup_backends_)
    {
        ASSERT_LE(num_backends, num_backup_backends_);

        for(size_t i = 0; i < num_backends; ++i)
        {
            setup_multi_dir(i);
        }
    }

    void
    destroy_multi_dir(size_t i)
    {
        fs::path& dir = multi_dirs_[i];
        if(fs::exists(dir))
        {
            fs::remove(dir);
        }
    }

    void
    destroy_multi_dirs(size_t num_backends = num_backup_backends_)
    {
        for(size_t i = 0; i < num_backends; ++i)
        {
            destroy_multi_dir(i);
        }

    }

    enum class Action { Add, Delete };

    void
    switch_backends()
    {
        std::vector<bool> status(num_backup_backends_, true);

        Chooser<Action> action_chooser(std::vector<std::pair<Action, unsigned> >(
            {{Action::Add, 1},
                {Action::Delete, 1}}));
        boost::random::discrete_distribution<> dist= {1,1,1};


        while(not stop_switcher)
        {
            boost::this_thread::sleep(boost::posix_time::seconds(1));
            switch(action_chooser())
            {
            case Action::Add:
                {
                    unsigned disk = dist(action_chooser.gen_);
                    if(not status[disk])
                    {
                        setup_multi_dir(disk);
                        status[disk] = true;
                    }
                }
                break;

            case Action::Delete:
                {
                    unsigned disk = dist(action_chooser.gen_);
                    if(status[disk])
                    {
                        destroy_multi_dir(disk);
                        status[disk] = false;
                    }

                }
                break;
            }
        }
    }

    void
    write_data(const size_t num_writes,
               BackendConnectionManagerPtr& manager,
               const fs::path& object_path)
    {
        BackendConnectionInterfacePtr connection = manager->getConnection();
        const Namespace ns(youtils::UUID().str());

        while(true)
        {
            try
            {
                ASSERT_FALSE(connection->namespaceExists(ns));
                break;
            }
            catch(std::exception&)
            {
            }
        }

        while(true)
        {
            try
            {
                connection->createNamespace(ns);
                break;
            }
            catch(std::exception& e)
            {
            }
        }
        while(true)
        {
            try
            {
                ASSERT_TRUE(connection->namespaceExists(ns));
                break;
            }
            catch(std::exception&)
            {
            }

        }

        size_t i = 0;
        while(i < num_writes)
        {
            std::stringstream ss;
            ss << i;

            try
            {
                connection->write(ns,
                                  object_path,
                                  ss.str());
                ++i;

            }
            catch(std::exception&)
            {
            }
        }

        i = 0;
        const fs::path object_to_read_to = youtils::FileUtils::create_temp_file_in_temp_dir("OBJECT");
        ALWAYS_CLEANUP_FILE(object_to_read_to);

        while(i < num_writes)
        {
            std::stringstream ss;
            ss << i;
            try
            {
                connection->read(ns,
                                 object_to_read_to,
                                 ss.str(),
                                 InsistOnLatestVersion::T);
                ++i;

            }
            catch(std::exception&)
            {
            }
        }

        i = 0;
        while(i < num_writes)
        {

            std::stringstream ss;
            ss << i;

            try
            {
                connection->remove(ns,
                                   ss.str());
                ++i;

            }
            catch(std::exception&)
            {
            }
        }
        while(true)
        {
            try
            {
                connection->deleteNamespace(ns);
                break;
            }
            catch(std::exception& e)
            {
            }
        }
        while(true)
        {
            try
            {
                ASSERT_FALSE(connection->namespaceExists(ns));
                break;
            }
            catch(std::exception& e)
            {
            }
        }
    }


    bpt::ptree
    make_local_config(size_t num_backends = num_backup_backends_)
    {
        bpt::ptree backend_connection_manager;
        backend_connection_manager.put("backend_type", "MULTI");

        for(size_t i =  0; i < num_backends; ++i)
        {
            std::stringstream ss;
            ss << i;

            bpt::ptree connection;
            connection.put("backend_type", "LOCAL");
            connection.put("local_connection_path", multi_dirs_[i].string());
            backend_connection_manager.put_child(ss.str(), connection);
        }

        bpt::ptree res;
        res.put_child("backend_connection_manager", backend_connection_manager);

        return res;
    }

    void
    configure_namespace_selector(bpt::ptree& pt,
                                 uint32_t error_policy = SwitchConnectionPoolOnErrorPolicy::OnBackendError)
    {
        const SwitchConnectionPoolPolicy policy = SwitchConnectionPoolPolicy::OnError;
        ip::PARAMETER_TYPE(backend_interface_switch_connection_pool_policy)(policy).persist(pt);
        ip::PARAMETER_TYPE(backend_interface_switch_connection_pool_on_error_policy)(error_policy).persist(pt);
    }

    void
    configure_round_robin_selector(bpt::ptree& pt)
    {
        const SwitchConnectionPoolPolicy policy = SwitchConnectionPoolPolicy::RoundRobin;
        ip::PARAMETER_TYPE(backend_interface_switch_connection_pool_policy)(policy).persist(pt);
    }

    void
    test_namespace_pool_selector_backend_error(bool switch_pool)
    {
        const size_t path_count = 2;
        bpt::ptree pt(make_local_config(path_count));

        configure_namespace_selector(pt,
                                     switch_pool ?
                                     SwitchConnectionPoolOnErrorPolicy::OnBackendError :
                                     0);

        BackendConnectionManagerPtr cm(BackendConnectionManager::create(pt));
        const Namespace nspace(yt::UUID().str());

        NamespacePoolSelector selector(*cm,
                                       nspace);

        std::shared_ptr<ConnectionPool> orig_pool(selector.pool());

        EXPECT_EQ(orig_pool,
                  selector.pool());

        selector.backend_error();

        std::shared_ptr<ConnectionPool> fallback_pool(selector.pool());
        if (switch_pool)
        {
            EXPECT_NE(orig_pool,
                      fallback_pool);
        }
        else
        {
            EXPECT_EQ(orig_pool,
                      fallback_pool);
        }

        EXPECT_EQ(fallback_pool,
                  selector.pool());

        EXPECT_FALSE(orig_pool->blacklisted());
        EXPECT_FALSE(fallback_pool->blacklisted());
    }

    using ConnectionPoolSet = std::set<std::shared_ptr<ConnectionPool>>;

    static ConnectionPoolSet
    connection_manager_pools(BackendConnectionManager& cm)
    {
        ConnectionPoolSet set;
        cm.visit_pools([&](std::shared_ptr<ConnectionPool> p)
                       {
                           bool ok = false;
                           std::tie(std::ignore, ok) = set.emplace(p);
                           EXPECT_TRUE(ok);
                       });

        return set;
    }

protected:
    std::vector<fs::path> multi_dirs_;
    static const size_t num_backup_backends_;

private:
    const fs::path multi_dir_base_;

protected:
    bool stop_switcher;
    // boost::chrono::seconds switcher_sleep_time_;
};

const size_t
MultiBackendTest::num_backup_backends_ = 8;

TEST_F(MultiBackendTest, empty_multi)
{
    bpt::ptree pt = make_local_config(0);
    std::unique_ptr<BackendConfig> config = BackendConfig::makeBackendConfig(pt);
    EXPECT_THROW(BackendConnectionManager::create(pt),
                 std::exception);
}

TEST_F(MultiBackendTest, no_different_types_in_multi)
{
    bpt::ptree pt = make_local_config(2);
    EXPECT_NO_THROW(BackendConfig::makeBackendConfig(pt));

    bpt::ptree connection;
    connection.put("backend_type", "ALBA");
    connection.put("alba_connection_host", "localhost");
    connection.put("alba_connection_port", "12345");

    pt.put_child("backend_connection_manager.2", connection);
    EXPECT_THROW(BackendConfig::makeBackendConfig(pt),
                 DifferentTypesInMultiException);

    EXPECT_THROW(BackendConnectionManager::create(pt),
                 DifferentTypesInMultiException);
}

TEST_F(MultiBackendTest, basics)
{
    const size_t n = 3;
    setup_multi_dirs(n);
    const bpt::ptree pt(make_local_config(n));
    const MultiConfig cfg(pt);
    BackendConnectionManagerPtr cm(BackendConnectionManager::create(pt));

    ConnectionPoolSet pools(connection_manager_pools(*cm));

    ASSERT_EQ(n,
              cfg.configs_.size());
    ASSERT_EQ(n,
              pools.size());

    for (const auto& c : cfg.configs_)
    {
        for (auto& p : pools)
        {
            if (*c == p->config())
            {
                pools.erase(p);
                break;
            }
        }
    }

    EXPECT_TRUE(pools.empty());
}

TEST_F(MultiBackendTest, pool_distribution)
{
    const size_t path_count =
        std::min(num_backup_backends_,
                 yt::System::get_env_with_default("MULTI_BACKEND_TEST_PATH_COUNT",
                                                  num_backup_backends_));
    const size_t nspace_count =
        yt::System::get_env_with_default("MULTI_BACKEND_TEST_NAMESPACE_COUNT",
                                         path_count * 100);
    setup_multi_dirs(path_count);
    const bpt::ptree pt(make_local_config(path_count));
    const MultiConfig cfg(pt);
    BackendConnectionManagerPtr cm(BackendConnectionManager::create(pt));

    const ConnectionPoolSet pools(connection_manager_pools(*cm));

    ASSERT_EQ(path_count,
              pools.size());

    std::map<std::shared_ptr<ConnectionPool>, size_t> dist;

    for (auto& p : pools)
    {
        ASSERT_EQ(0, p->size());
        dist[p] = 0;
    }

    for (size_t i = 0; i < nspace_count; ++i)
    {
        const Namespace nspace(yt::UUID().str());
        BackendConnectionInterfacePtr
            c(cm->getConnection(ForceNewConnection::F,
                                nspace));
        ASSERT_TRUE(c != nullptr);
        auto it = dist.find(cm->pool(nspace));
        ASSERT_TRUE(it != dist.end());
        it->second += 1;
    }

    for (auto& p : pools)
    {
        ASSERT_EQ(1, p->size());
    }

    for (auto& p : dist)
    {
        std::cout << "pool " << p.first << ": " << p.second << " nspaces" << std::endl;
    }
}

TEST_F(MultiBackendTest, failover)
{
    const size_t path_count = 2;
    bpt::ptree pt(make_local_config(path_count));

    const size_t blacklist_secs = 1;
    ip::PARAMETER_TYPE(backend_connection_pool_blacklist_secs)(blacklist_secs).persist(pt);

    BackendConnectionManagerPtr cm(BackendConnectionManager::create(pt));
    const Namespace nspace(yt::UUID().str());

    auto get_pool([&]
                  {
                      return cm->getConnection(ForceNewConnection::F,
                                               nspace).get_deleter().pool();
                  });

    std::shared_ptr<ConnectionPool> orig_pool(get_pool());
    EXPECT_EQ(orig_pool,
              get_pool());

    inject_error_into_connection_pool(*orig_pool);

    std::shared_ptr<ConnectionPool> fallback_pool(get_pool());
    EXPECT_NE(orig_pool,
              fallback_pool);
    EXPECT_EQ(fallback_pool,
              get_pool());

    boost::this_thread::sleep_for(bc::seconds(blacklist_secs));

    EXPECT_EQ(orig_pool,
              get_pool());
}

TEST_F(MultiBackendTest, namespace_pool_selector_happy_path)
{
    const size_t path_count = 2;
    bpt::ptree pt(make_local_config(path_count));
    configure_namespace_selector(pt);

    const size_t blacklist_secs = 1;
    ip::PARAMETER_TYPE(backend_connection_pool_blacklist_secs)(blacklist_secs).persist(pt);

    BackendConnectionManagerPtr cm(BackendConnectionManager::create(pt));
    const Namespace nspace(yt::UUID().str());

    NamespacePoolSelector selector(*cm,
                                   nspace);

    EXPECT_EQ(selector.pool(),
              selector.pool());
}

TEST_F(MultiBackendTest, namespace_pool_selector_conn_error)
{
    const size_t path_count = 2;
    bpt::ptree pt(make_local_config(path_count));
    configure_namespace_selector(pt);

    const size_t blacklist_secs = 1;
    ip::PARAMETER_TYPE(backend_connection_pool_blacklist_secs)(blacklist_secs).persist(pt);

    BackendConnectionManagerPtr cm(BackendConnectionManager::create(pt));
    const Namespace nspace(yt::UUID().str());

    NamespacePoolSelector selector(*cm,
                                   nspace);

    std::shared_ptr<ConnectionPool> orig_pool(selector.pool());

    EXPECT_EQ(orig_pool,
              selector.pool());

    selector.connection_error();

    std::shared_ptr<ConnectionPool> fallback_pool(selector.pool());
    EXPECT_NE(orig_pool,
              fallback_pool);
    EXPECT_EQ(fallback_pool,
              selector.pool());

    EXPECT_TRUE(orig_pool->blacklisted());
    EXPECT_FALSE(fallback_pool->blacklisted());

    boost::this_thread::sleep_for(bc::seconds(blacklist_secs));

    EXPECT_FALSE(orig_pool->blacklisted());
    EXPECT_EQ(orig_pool,
              selector.pool());
}

TEST_F(MultiBackendTest, namespace_pool_selector_backend_error_1)
{
    test_namespace_pool_selector_backend_error(true);
}

TEST_F(MultiBackendTest, namespace_pool_selector_backend_error_2)
{
    test_namespace_pool_selector_backend_error(false);
}

namespace
{

using PoolSet = std::set<std::shared_ptr<ConnectionPool>>;

}

TEST_F(MultiBackendTest, round_robin_selector_happy_path)
{
    const size_t path_count = 4;
    bpt::ptree pt(make_local_config(path_count));
    configure_round_robin_selector(pt);

    const size_t blacklist_secs = 1;
    ip::PARAMETER_TYPE(backend_connection_pool_blacklist_secs)(blacklist_secs).persist(pt);
    BackendConnectionManagerPtr cm(BackendConnectionManager::create(pt));

    ASSERT_EQ(path_count,
              cm->pools().size());

    RoundRobinPoolSelector selector(*cm);

    PoolSet pools;

    for (size_t i = 0; i < path_count; ++i)
    {
        bool ok = false;
        std::tie(std::ignore, ok) = pools.insert(selector.pool());
        EXPECT_TRUE(ok);
    }

    EXPECT_EQ(path_count,
              pools.size());
}

TEST_F(MultiBackendTest, round_robin_selector_pool_blacklisted)
{
    const size_t path_count = 4;
    bpt::ptree pt(make_local_config(path_count));
    configure_round_robin_selector(pt);

    const size_t blacklist_secs = 1;
    ip::PARAMETER_TYPE(backend_connection_pool_blacklist_secs)(blacklist_secs).persist(pt);

    BackendConnectionManagerPtr cm(BackendConnectionManager::create(pt));

    ASSERT_EQ(path_count,
              cm->pools().size());

    RoundRobinPoolSelector selector(*cm);

    cm->pools().front()->error();
    cm->pools().back()->error();

    PoolSet pools;

    for (size_t i = 0; i < path_count; ++i)
    {
        pools.insert(selector.pool());
    }

    EXPECT_EQ(path_count - 2,
              pools.size());
}

TEST_F(MultiBackendTest, DISABLED_stress)
{

    const fs::path object = youtils::FileUtils::create_temp_file_in_temp_dir("OBJECT");
    ALWAYS_CLEANUP_FILE(object);
    {

        fs::ofstream of(object);
        std::string str(1024,'\0');
        of << str;
    }
    EXPECT_TRUE(fs::file_size(object) == 1024);


    setup_multi_dirs();
    bpt::ptree pt = make_local_config();
    BackendConnectionManagerPtr bcm = BackendConnectionManager::create(pt);
    BackendConnectionInterfacePtr connection = bcm->getConnection();


    const unsigned number_of_writes_per_thread = 1024 ;
    const unsigned number_of_threads = 64;


    boost::thread t([this]()
                    {
                        switch_backends();
                    }
                    );

    auto writer([number_of_writes_per_thread,
                 bcm,
                 &object,
                 this]() mutable
                {
                    write_data(number_of_writes_per_thread,
                               bcm,
                               object);
                });

    std::vector<boost::thread*> threads;

    for(unsigned i = 0; i < number_of_threads; ++i)
    {
        threads.push_back(new boost::thread(writer));
    }

    for(unsigned i = 0; i < threads.size(); ++i)
    {
        threads[i]->join();
        delete threads[i];
        threads[i] = 0;
    }

    stop_switcher = true;
    t.join();

}
}

// Local Variables: **
// mode: c++ **
// End: **
