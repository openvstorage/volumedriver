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

#include "FileSystemTestBase.h"

#include "../LocalPythonClient.h"
#include "../PythonClient.h"

#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/scope_exit.hpp>

#include <youtils/Logger.h>
#include <youtils/Logging.h>

#include <volumedriver/Api.h>

namespace volumedriverfstest
{

namespace bpt = boost::property_tree;
namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace vd = volumedriver;
namespace vfs = volumedriverfs;
namespace yt = youtils;

class LocalPythonClientTest
    : public FileSystemTestBase
{
protected:
    std::unique_ptr<vfs::LocalPythonClient> local_client_;

    LocalPythonClientTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("LocalLocalPythonClientTest"))
    {
        Py_Initialize();
    }

    virtual ~LocalPythonClientTest()
    {
        Py_Finalize();
    }


    virtual void
    SetUp()
    {
        FileSystemTestBase::SetUp();
        local_client_.reset(new vfs::LocalPythonClient(configuration_.string()));
    }

    void
    test_general_logging_level(yt::Severity sev)
    {
        if (yt::Logger::loggingEnabled())
        {
            const yt::Severity old_sev = yt::Logger::generalLogging();

            BOOST_SCOPE_EXIT((old_sev))
            {
                yt::Logger::generalLogging(old_sev);
            }
            BOOST_SCOPE_EXIT_END;

            local_client_->set_general_logging_level(sev);
            EXPECT_EQ(sev, local_client_->get_general_logging_level());
        }
        else
        {
            std::cout <<
                "Logging is disabled. Re-run with logging enabled for better coverage." <<
                std::endl;

            EXPECT_THROW(local_client_->set_general_logging_level(sev),
                         std::exception);
            EXPECT_THROW(local_client_->get_general_logging_level(),
                         std::exception);
        }
    }

    DECLARE_LOGGER("LocalPythonClientTest");
};

TEST_F(LocalPythonClientTest, get_running_configuration)
{
    EXPECT_EQ(api::persistConfiguration(true), local_client_->get_running_configuration(true));
    EXPECT_EQ(api::persistConfiguration(false), local_client_->get_running_configuration(false));
}

TEST_F(LocalPythonClientTest, update_configuration)
{
    std::stringstream ss(local_client_->get_running_configuration(false));

    bpt::ptree pt;
    bpt::json_parser::read_json(ss, pt);

    // a resettable one, arbitrarily chosen:
    ip::PARAMETER_TYPE(fs_ignore_sync) old_ignore_sync(pt);

    ASSERT_TRUE(decltype(old_ignore_sync)::resettable());
    ip::PARAMETER_TYPE(fs_ignore_sync) new_ignore_sync(not old_ignore_sync.value());

    new_ignore_sync.persist(pt);

    // and a non-resetable one, also arbitrarily chosen:
    ip::PARAMETER_TYPE(fs_max_open_files) old_max_open_files(pt);

    ASSERT_FALSE(decltype(old_max_open_files)::resettable());
    ip::PARAMETER_TYPE(fs_max_open_files) new_max_open_files(old_max_open_files.value() + 1);

    new_max_open_files.persist(pt);

    const fs::path path(topdir_ / "updated-config.json");
    bpt::json_parser::write_json(path.string(),
                                 pt);

    const yt::UpdateReport report(local_client_->update_configuration(path.string()));

    ASSERT_EQ(1U, report.update_size());
    ASSERT_EQ(new_ignore_sync.name(), report.getUpdates().front().parameter_name);

    ip::PARAMETER_TYPE(num_threads) old_num_threads(pt);

    // num_threads will complain if a certain limit (256 at the time of writing) is
    // exceeded
    typedef decltype(old_num_threads) nthreads_t;
    ASSERT_TRUE(nthreads_t::resettable());

    nthreads_t new_num_threads(std::numeric_limits<nthreads_t::ValueType>::max());
    new_num_threads.persist(pt);

    bpt::json_parser::write_json(path.string(),
                                 pt);

    ASSERT_THROW(local_client_->update_configuration(path.string()),
                 vfs::clienterrors::ConfigurationUpdateException);
}

TEST_F(LocalPythonClientTest, general_loglevel)
{
    test_general_logging_level(yt::Severity::trace);
    test_general_logging_level(yt::Severity::debug);
    test_general_logging_level(yt::Severity::info);
    test_general_logging_level(yt::Severity::warning);
    test_general_logging_level(yt::Severity::error);
    test_general_logging_level(yt::Severity::fatal);
    test_general_logging_level(yt::Severity::notification);
}

TEST_F(LocalPythonClientTest, logging_filters)
{
    if (yt::Logger::loggingEnabled())
    {
        {
            std::vector<yt::Logger::filter_t> filters;
            yt::Logger::all_filters(filters);
            ASSERT_TRUE(filters.empty())  <<
                "bailing out as there are logging filters configured and I don't want to mess them up";
        }

        int dummy = 0;

        BOOST_SCOPE_EXIT((dummy))
        {
            (void) dummy;
            yt::Logger::remove_all_filters();
        }
        BOOST_SCOPE_EXIT_END;

        EXPECT_TRUE(local_client_->get_logging_filters().empty());
        EXPECT_NO_THROW(local_client_->remove_logging_filters());

        const yt::Logger::filter_t filter1("Match1", yt::Severity::fatal);
        local_client_->add_logging_filter(filter1.first, filter1.second);

        const yt::Logger::filter_t filter2("Match2", yt::Severity::error);
        local_client_->add_logging_filter(filter2.first, filter2.second);

        EXPECT_EQ(2U, local_client_->get_logging_filters().size());

        local_client_->remove_logging_filter(filter1.first);

        {
            const std::vector<yt::Logger::filter_t> rfilters(local_client_->get_logging_filters());
            EXPECT_EQ(1U, rfilters.size());
            EXPECT_TRUE(filter2 == rfilters[0]);
        }

        EXPECT_NO_THROW(local_client_->remove_logging_filter(filter1.first));

        local_client_->remove_logging_filters();

        EXPECT_TRUE(local_client_->get_logging_filters().empty());
    }
    else
    {
        std::cout <<
            "Logging is disabled. Re-run with logging enabled for better coverage." <<
            std::endl;
    }
}

TEST_F(LocalPythonClientTest, malloc_info)
{
    const std::string mi(local_client_->malloc_info());
    EXPECT_FALSE(mi.empty());

    LOG_INFO(mi);
}

TEST_F(LocalPythonClientTest, restart_volume)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));

    const vfs::ObjectId oid(create_file(fname,
                                        1ULL << 20));

    const std::string pattern("a rather important message");

    write_to_file(fname,
                  pattern.c_str(),
                  pattern.size(),
                  0);

    local_client_->stop_object(oid.str());

    std::vector<char> buf(pattern.size());

    ASSERT_GT(0,
              read_from_file(fname,
                             buf.data(),
                             buf.size(),
                             0));

    const std::string pattern2("a much less important message");
    ASSERT_GT(0,
              write_to_file(fname,
                            pattern2.c_str(),
                            pattern2.size(),
                            0));

    local_client_->restart_object(oid.str(),
                                  false);
    read_from_file(fname,
                   buf.data(),
                   buf.size(),
                   0);

    const std::string s(buf.data(),
                        buf.size());

    ASSERT_EQ(pattern,
              s);
}

TEST_F(LocalPythonClientTest, cluster_cache_handles)
{
    const vfs::FrontendPath vpath(make_volume_name("/cluster-cache-handles-test"));
    const std::string vname(create_file(vpath, 10 << 20));

    auto check_content_based_map([&]
                                 {
                                     std::vector<vd::ClusterCacheHandle>
                                         v(local_client_->list_cluster_cache_handles());
                                     ASSERT_EQ(1U,
                                               v.size());
                                     EXPECT_EQ(vd::ClusterCacheHandle(0),
                                               v[0]);
                                 });

    check_content_based_map();

    local_client_->set_cluster_cache_mode(vname,
                                   vd::ClusterCacheMode::LocationBased);
    const uint64_t limit = 13;
    local_client_->set_cluster_cache_limit(vname,
                                    vd::ClusterCount(limit));

    const std::vector<vd::ClusterCacheHandle> v(local_client_->list_cluster_cache_handles());
    ASSERT_EQ(2U,
              v.size());

    for (const auto& h : v)
    {
        const vfs::XMLRPCClusterCacheHandleInfo
            info(local_client_->get_cluster_cache_handle_info(h));
        EXPECT_EQ(h,
                  info.cluster_cache_handle);
        EXPECT_EQ(0U,
                  info.entries);

        if (h == vd::ClusterCacheHandle(0))
        {
            EXPECT_EQ(boost::none,
                      info.max_entries);
            EXPECT_THROW(local_client_->remove_cluster_cache_handle(h),
                         std::exception);

        }
        else
        {
            EXPECT_EQ(limit,
                      *info.max_entries);
            local_client_->remove_cluster_cache_handle(h);
            EXPECT_THROW(local_client_->get_cluster_cache_handle_info(h),
                         std::exception);
        }
    }

    check_content_based_map();
}

}
