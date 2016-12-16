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

#include <boost/chrono.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/thread/thread.hpp>

#include <backend/Multi_Connection.h>
#include <youtils/Chooser.h>
#include <youtils/FileUtils.h>
#include <youtils/UUID.h>

namespace backendtest
{

using namespace backend;
using youtils::Chooser;

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;

class MultiBackendTest :
        public BackendTestBase
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
        fs::path& dir = multi_dirs_[i];

        if(not fs::exists(dir))
        {
            fs::create_symlink(multi_dir_base_, dir);
        }
    }

    void
    setup_multi_dirs(size_t num_backends = num_backup_backends_)
    {

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
protected:
    std::vector<fs::path> multi_dirs_;
    static const size_t num_backup_backends_ = 3;

private:
    const fs::path multi_dir_base_;
protected:
    bool stop_switcher;
    // boost::chrono::seconds switcher_sleep_time_;


};

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
