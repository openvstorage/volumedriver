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


    boost::property_tree::ptree
    make_local_config(size_t num_backends = num_backup_backends_)
    {
        boost::property_tree::ptree backend_connection_manager;
        backend_connection_manager.put("backend_type", "MULTI");

        for(size_t i =  0; i < num_backends; ++i)
        {
            std::stringstream ss;
            ss << i;

            boost::property_tree::ptree connection;
            connection.put("backend_type", "LOCAL");
            connection.put("local_connection_path", multi_dirs_[i].string());
            backend_connection_manager.put_child(ss.str(), connection);
        }

        boost::property_tree::ptree res;
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
    boost::property_tree::ptree pt = make_local_config(0);
    std::unique_ptr<BackendConfig> config = BackendConfig::makeBackendConfig(pt);

    BackendConnectionManagerPtr  bcm = BackendConnectionManager::create(pt);

    BackendConnectionInterfacePtr connection = bcm->getConnection();
    std::list<std::string> any_result;

    EXPECT_THROW(connection->listNamespaces(any_result),
                     BackendNoMultiBackendAvailableException);
    EXPECT_THROW(connection->listObjects(Namespace(std::string("anything")),
                                         any_result),
                 BackendNoMultiBackendAvailableException);

    EXPECT_THROW(connection->namespaceExists(std::string("anything")),
                 BackendNoMultiBackendAvailableException);

    EXPECT_THROW(connection->createNamespace(std::string("anything")),
                 BackendNoMultiBackendAvailableException);

    EXPECT_THROW(connection->deleteNamespace(std::string("anything")),
                 BackendNoMultiBackendAvailableException);


    EXPECT_THROW(connection->read(Namespace(std::string("anything")),
                                  "anything",
                                  "anything",
                                  InsistOnLatestVersion::T),
                 BackendNoMultiBackendAvailableException);

    EXPECT_THROW(connection->write(Namespace(std::string("anything")),
                                   "anything",
                                   "anything"),
                 BackendNoMultiBackendAvailableException);

    EXPECT_THROW(connection->objectExists(Namespace(std::string("anything")),
                                          "anything"),
                 BackendNoMultiBackendAvailableException);

    EXPECT_THROW(connection->remove(Namespace(std::string("anything")),
                                    "anything"),
                 BackendNoMultiBackendAvailableException);

    EXPECT_THROW(connection->getSize(Namespace(std::string("anything")),
                                     "anything"),
                 BackendNoMultiBackendAvailableException);

    EXPECT_THROW(connection->getCheckSum(Namespace(std::string("anyting")),
                                         "anything"),
                 BackendNoMultiBackendAvailableException);
}

// AR: disabled while we only have a single backend type (LOCAL) that
// MultiBackend can cope with
TEST_F(MultiBackendTest, DISABLED_no_different_types_in_multi)
{
    boost::property_tree::ptree pt = make_local_config(2);
    EXPECT_NO_THROW(BackendConfig::makeBackendConfig(pt));

    // ALBA is not supported by MultiBackend - if you reanimate this
    // test use a backend that actually is supported / make ALBA supported.
    boost::property_tree::ptree connection;
    connection.put("backend_type", "ALBA");
    connection.put("alba_connection_host", "localhost");
    connection.put("alba_connection_port", "12345");

    pt.put_child("backend_connection_manager.2", connection);
    EXPECT_THROW(BackendConfig::makeBackendConfig(pt),
                 DifferentTypesInMultiException);

    EXPECT_THROW(BackendConnectionManager::create(pt),
                 DifferentTypesInMultiException);
}

TEST_F(MultiBackendTest, config_reading)
{
    boost::property_tree::ptree pt = make_local_config();
    std::unique_ptr<BackendConfig> config = BackendConfig::makeBackendConfig(pt);

    for(size_t i = 0; i < num_backup_backends_; ++i)
    {
        setup_multi_dir(i);
    }
    for(size_t i = 0; i < num_backup_backends_; ++i)
    {
        destroy_multi_dir(i);
    }
    // This doesn't work anymore because of redlicha da98615dfe22:
    // existence of the directory is checked on creation already
    // setup_multi_dir(0);
    // BackendConnectionManagerPtr bcm = BackendConnectionManager::create(pt);
    // BackendConnectionInterfacePtr connection = bcm->getConnection();

    // EXPECT_NO_THROW(connection->namespaceExists(Namespace(std::string("anything"))));

    // EXPECT_NO_THROW(connection->createNamespace(Namespace(std::string("anything"))));
}

TEST_F(MultiBackendTest, namespace_operations)
{
    boost::property_tree::ptree pt = make_local_config();
    setup_multi_dirs();
    BackendConnectionManagerPtr bcm = BackendConnectionManager::create(pt);
    BackendConnectionInterfacePtr connection = bcm->getConnection();
    const Namespace ns(youtils::UUID().str());
    for(size_t i = 0; i < num_backup_backends_; ++i)
    {
        std::list<std::string> nss;
        ASSERT_NO_THROW(connection->listNamespaces(nss));
        EXPECT_TRUE(nss.empty());

        bool ns_exists = false;
        ASSERT_NO_THROW(ns_exists = connection->namespaceExists(ns));
        EXPECT_FALSE(ns_exists);

        ASSERT_NO_THROW(connection->createNamespace(ns));
        ASSERT_NO_THROW(connection->listNamespaces(nss));
        EXPECT_TRUE(nss.size() == 1);
        EXPECT_TRUE(nss.front() == ns.str());
        nss.clear();

        ASSERT_NO_THROW(ns_exists = connection->namespaceExists(ns));
        EXPECT_TRUE(ns_exists);


        ASSERT_NO_THROW(connection->deleteNamespace(ns));
        ASSERT_NO_THROW(connection->listNamespaces(nss));
        EXPECT_TRUE(nss.empty());
        ASSERT_NO_THROW(ns_exists = connection->namespaceExists(ns));
        EXPECT_FALSE(ns_exists);

        destroy_multi_dir(i);
    }

    {
        std::list<std::string> nss;
        ASSERT_THROW(connection->listNamespaces(nss),
                     std::exception);

        ASSERT_THROW(connection->namespaceExists(ns),
                     std::exception);
        ASSERT_THROW(connection->createNamespace(ns),
                     std::exception);
    }

    {
        setup_multi_dir(2);
        std::list<std::string> nss;
        ASSERT_NO_THROW(connection->listNamespaces(nss));
        EXPECT_TRUE(nss.empty());
        bool ns_exists = false;
        ASSERT_NO_THROW(ns_exists = connection->namespaceExists(ns));
        EXPECT_FALSE(ns_exists);

        ASSERT_NO_THROW(connection->createNamespace(ns));
        ASSERT_NO_THROW(connection->listNamespaces(nss));
        EXPECT_TRUE(nss.size() == 1);
        EXPECT_TRUE(nss.front() == ns.str());
        nss.clear();

        ASSERT_NO_THROW(ns_exists = connection->namespaceExists(ns));
        EXPECT_TRUE(ns_exists);


        ASSERT_NO_THROW(connection->deleteNamespace(ns));
        ASSERT_NO_THROW(connection->listNamespaces(nss));
        EXPECT_TRUE(nss.empty());
        ASSERT_NO_THROW(ns_exists = connection->namespaceExists(ns));
        EXPECT_FALSE(ns_exists);

        destroy_multi_dir(2);

    }
}

TEST_F(MultiBackendTest, read_write_create_operations)
{

    boost::property_tree::ptree pt = make_local_config();
    setup_multi_dirs();
    BackendConnectionManagerPtr bcm = BackendConnectionManager::create(pt);
    BackendConnectionInterfacePtr connection = bcm->getConnection();
    const Namespace ns(youtils::UUID().str());

    ASSERT_NO_THROW(connection->createNamespace(ns));
    ASSERT_TRUE(connection->namespaceExists(ns));
    const std::string obj("OBJECT");
    fs::path obj_path(youtils::FileUtils::create_temp_file_in_temp_dir(obj));
    std::list<std::string> objs;
    for(size_t i = 0; i < num_backup_backends_; ++i)
    {

        ASSERT_NO_THROW(connection->listObjects(ns,
                                                objs));
        EXPECT_TRUE(objs.empty());
        bool obj_exists = false;
        ASSERT_NO_THROW(obj_exists = connection->objectExists(ns, obj));
        EXPECT_FALSE(obj_exists);

        ASSERT_NO_THROW(connection->write(ns,
                                          obj_path,
                                          obj,
                                          OverwriteObject::F));

        ASSERT_NO_THROW(connection->listObjects(ns,
                                                objs));
        EXPECT_TRUE(objs.size() == 1);
        EXPECT_TRUE(objs.front() == obj);
        objs.clear();

        ASSERT_NO_THROW(obj_exists = connection->objectExists(ns,
                                                              obj));
        EXPECT_TRUE(obj_exists);
        fs::path obj_path(youtils::FileUtils::create_temp_file_in_temp_dir(obj));
        ASSERT_NO_THROW(connection->read(ns,
                                         obj_path,
                                         obj,
                                         InsistOnLatestVersion::T));
        ASSERT_NO_THROW(connection->remove(ns,
                                           obj,
                                           ObjectMayNotExist::T));
        ASSERT_NO_THROW(connection->listObjects(ns,
                                                objs));
        EXPECT_TRUE(objs.empty());
        ASSERT_NO_THROW(obj_exists = connection->objectExists(ns, obj));
        EXPECT_FALSE(obj_exists);
        destroy_multi_dir(i);
    }
    {

        ASSERT_THROW(connection->listObjects(ns,
                                             objs),
                     std::exception);

        ASSERT_THROW(connection->objectExists(ns, obj),
                     std::exception);


        ASSERT_THROW(connection->write(ns,
                                       obj_path,
                                       obj,
                                       OverwriteObject::F),
                     std::exception);

        ASSERT_THROW(connection->listObjects(ns,
                                             objs),
                     std::exception);
        ASSERT_THROW(connection->objectExists(ns,
                                              obj),
                     std::exception);

        fs::path obj_path(youtils::FileUtils::create_temp_file_in_temp_dir(obj));
        ASSERT_THROW(connection->read(ns,
                                      obj_path,
                                      obj,
                                      InsistOnLatestVersion::T),
                     std::exception);
        ASSERT_THROW(connection->remove(ns,
                                        obj,
                                        ObjectMayNotExist::T),
                     std::exception);
        ASSERT_THROW(connection->listObjects(ns,
                                             objs),
                     std::exception);

        ASSERT_THROW(connection->objectExists(ns, obj),
                     std::exception);
    }
    {
        setup_multi_dir(1);

        ASSERT_NO_THROW(connection->listObjects(ns,
                                                objs));
        EXPECT_TRUE(objs.empty());
        bool obj_exists = false;
        ASSERT_NO_THROW(obj_exists = connection->objectExists(ns, obj));
        EXPECT_FALSE(obj_exists);

        ASSERT_NO_THROW(connection->write(ns,
                                          obj_path,
                                          obj,
                                          OverwriteObject::F));

        ASSERT_NO_THROW(connection->listObjects(ns,
                                                objs));
        EXPECT_TRUE(objs.size() == 1);
        EXPECT_TRUE(objs.front() == obj);
        objs.clear();

        ASSERT_NO_THROW(obj_exists = connection->objectExists(ns,
                                                              obj));
        EXPECT_TRUE(obj_exists);
        fs::path obj_path(youtils::FileUtils::create_temp_file_in_temp_dir(obj));
        ASSERT_NO_THROW(connection->read(ns,
                                         obj_path,
                                         obj,
                                         InsistOnLatestVersion::T));
        ASSERT_NO_THROW(connection->remove(ns,
                                           obj,
                                           ObjectMayNotExist::T));
        ASSERT_NO_THROW(connection->listObjects(ns,
                                                objs));
        EXPECT_TRUE(objs.empty());
        ASSERT_NO_THROW(obj_exists = connection->objectExists(ns, obj));
        EXPECT_FALSE(obj_exists);
    }
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
    boost::property_tree::ptree pt = make_local_config();
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
