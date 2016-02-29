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

#include "VolManagerTestSetup.h"

#include <boost/property_tree/json_parser.hpp>

#include <youtils/BuildInfoString.h>
#include <youtils/FileUtils.h>
#include <youtils/InitializedParam.h>
#include <youtils/VolumeDriverComponent.h>

#include <backend/BackendConfig.h>

#include "../VolManager.h"
#include "../VolumeDriverParameters.h"

namespace volumedrivertest
{

using namespace volumedriver;

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace yt = youtils;

class VolumeDriverConfigurationTest
    : public VolManagerTestSetup
{
public:
    VolumeDriverConfigurationTest()
        : VolManagerTestSetup("VolumeDriverConfigurationTest")
    {}

protected:
    DECLARE_LOGGER("VolumeDriverConfigurationTest");

    static const uint32_t current_configuration = 1;

    template<typename T>
    void
    test_forbidden_param_update(const T& newval)
    {
        bpt::ptree pt;
        VolManager& vm = *VolManager::get();
        vm.persistConfiguration(pt,
                                ReportDefault::T);
        const T oldval(pt);

        ASSERT_NE(oldval.value(),
                  newval.value());

        newval.persist(pt);

        UpdateReport urep;
        ConfigurationReport crep;
        EXPECT_FALSE(vm.updateConfiguration(pt,
                                            urep,
                                            crep));
        EXPECT_EQ(1U,
                  crep.size());
        EXPECT_EQ(0U,
                  urep.update_size());

        vm.persistConfiguration(pt,
                                ReportDefault::T);
        const T latestval(pt);

        EXPECT_EQ(oldval.value(),
                  latestval.value());
    }
};

TEST_P(VolumeDriverConfigurationTest, checkPersistIsOkForUpdate)
{
    bpt::ptree pt;
    VolManager::get()->persistConfiguration(pt);
}

TEST_P(VolumeDriverConfigurationTest, DISABLED_checkConfiguration)
{
    bpt::ptree pt;

    ip::PARAMETER_TYPE(open_scos_per_volume)(0).persist(pt);

    ConfigurationReport rep;

    ASSERT_FALSE(VolManager::get()->checkConfiguration(pt,
                                                       rep));
    ASSERT_EQ(1U, rep.size());
    EXPECT_EQ(std::string(rep.front().param_name()),std::string("open_scos_per_volume"));
}

TEST_P(VolumeDriverConfigurationTest, PrintConfigurationDocumentation)
{
    std::cout <<
        "This documentation only documents the volumedriver *library*." <<
        std::endl <<
        "It was created for: " <<
        std::endl <<
        youtils::buildInfoString() <<
        "Some parameters might not be documented... these should be regarded as internal and should *not* be changed" <<
        std::endl;

    for (const auto pinfo: ip::ParameterInfo::get_parameter_info_list())
    {
        if(T(pinfo->document_))
        {

            std::cout << "* " << pinfo->name << std::endl
                      << "** " <<"section: " <<  pinfo->section_name << std::endl
                      << "** " << pinfo->doc_string << std::endl;
            if(pinfo->has_default)
            {
                std::cout << "** " << "default: " << pinfo->def_ << std::endl;
            }
            else
            {
                std::cout << "** " << "required parameter " << std::endl;
            }

            if(pinfo->dimensioned_value)
            {
                std::cout << "** Dimensioned Literal (\"234MiB\")" << std::endl;
            }
            else
            {
                std::cout << "** Normal Literal " << std::endl;
            }

            if(pinfo->can_be_reset)
            {
                std::cout << "** " << "Parameter can be reconfigured dynamically" << std::endl;
            }
            else
            {
                std::cout << "** " << "Parameter cannot be reconfigured dynamically" << std::endl;
            }
        }
    }
}

TEST_P(VolumeDriverConfigurationTest, DISABLED_dumpConfigAndExit)
{
    bpt::ptree pt;
    VolManager::get()->persistConfiguration(std::string("/tmp/current_configuration"));
    abort();
}

namespace
{

struct ComponentCounter
{
    ComponentCounter()
        :count_(0)
    {}

    void
    operator()(const VolumeDriverComponent* component)
    {
        ++count_;
        std::cout << component->componentName() << std::endl;
    }
    uint32_t count_;
};

}

TEST_P(VolumeDriverConfigurationTest, componentTest)
{
    ComponentCounter counter;
    VolManager::get()->for_each_components(counter);
    // Maintain a list of expected components here?
    ASSERT_EQ(8U, counter.count_);
}

TEST_P(VolumeDriverConfigurationTest, mount_points)
{
    {
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);
        ConfigurationReport c_rep;
        ASSERT_TRUE(VolManager::get()->getClusterCache().checkConfig(pt,
                                                                c_rep));
        ASSERT_EQ(0U, c_rep.size());
    }

    {
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);
        UpdateReport u_rep;

        ASSERT_NO_THROW(VolManager::get()->getClusterCache().update(pt,
                                                                    u_rep));
        EXPECT_EQ(3U, u_rep.getNoUpdates().size());
        EXPECT_EQ(0U, u_rep.getUpdates().size());
    }

    {
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);
        ConfigurationReport c_rep;
        ip::PARAMETER_TYPE(clustercache_mount_points) mps(pt);
        ip::PARAMETER_TYPE(clustercache_mount_points)::ValueType mount_vec;
        for (const auto& mp: mps.value())
        {
            mount_vec.push_back(mp);
        }
        mount_vec.push_back(MountPointConfig("/some/stuff", 45000));
        ip::PARAMETER_TYPE(clustercache_mount_points) new_mps(mount_vec);
        new_mps.persist(pt);

        ASSERT_FALSE(VolManager::get()->getClusterCache().checkConfig(pt,
                                                                 c_rep));
        ASSERT_EQ(1U, c_rep.size());
    }

    {
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);
        ConfigurationReport c_rep;
        bool add = false;
        ip::PARAMETER_TYPE(clustercache_mount_points) mps(pt);
        ip::PARAMETER_TYPE(clustercache_mount_points)::ValueType mount_vec;

        for (const auto& mp: mps.value())
        {
            if(add)
            {
                mount_vec.push_back(mp);
            }
            else
            {
                add = true;
            }
        }
        ip::PARAMETER_TYPE(clustercache_mount_points) new_mps(mount_vec);
        new_mps.persist(pt);

        ASSERT_FALSE(VolManager::get()->getClusterCache().checkConfig(pt,
                                                                 c_rep));
        ASSERT_EQ(1U, c_rep.size());
    }

    {
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);
        ConfigurationReport c_rep;
        bool add = false;
        ip::PARAMETER_TYPE(clustercache_mount_points) mps(pt);
        ip::PARAMETER_TYPE(clustercache_mount_points)::ValueType mount_vec;

        for (const auto& mp: mps.value())
        {
            if(add)
            {
                mount_vec.push_back(mp);
            }
            else
            {
                add = true;
            }
        }
        mount_vec.push_back(MountPointConfig("/some/stuff", 45000));
        ip::PARAMETER_TYPE(clustercache_mount_points) new_mps(mount_vec);
        new_mps.persist(pt);

        ASSERT_FALSE(VolManager::get()->getClusterCache().checkConfig(pt,
                                                                 c_rep));
        EXPECT_EQ(2U, c_rep.size());
    }

    {
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);
        ConfigurationReport c_rep;
        ip::PARAMETER_TYPE(clustercache_mount_points) mps(pt);
        ip::PARAMETER_TYPE(clustercache_mount_points)::ValueType mount_vec;


        for (const auto& mp: mps.value())
        {
            mount_vec.push_back(MountPointConfig(mp.path, 8943));
        }

        ip::PARAMETER_TYPE(clustercache_mount_points) new_mps(mount_vec);
        new_mps.persist(pt);

        ASSERT_FALSE(VolManager::get()->getClusterCache().checkConfig(pt,
                                                                 c_rep));
        EXPECT_EQ(2U, c_rep.size());
        for (const auto& msg: c_rep)
        {
            std::cout << msg.problem() << std::endl;
        }
    }

    {
        // This tests proper reading of offlined mountpoints
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);

        ClusterCache::ManagerType::Info device_info;
        VolManager::get()->getClusterCache().deviceInfo(device_info);
        EXPECT_EQ(2U, device_info.size());
        VolumeId vid1("volume1");
        auto ns1_ptr = make_random_namespace();
        const Namespace& ns1 = ns1_ptr->ns();

        //        Namespace ns1;
        Volume* v = newVolume(vid1,
                              ns1);
        const std::string pattern("bart");

        uint64_t hits = 0, misses = 0, entries = 0;

        VolManager::get()->getClusterCache().get_stats(hits, misses, entries);
        EXPECT_EQ(0U, entries);
        EXPECT_EQ(0U, hits);
        EXPECT_EQ(0U, misses);

        writeToVolume(v, 0, 4096, pattern);
        const uint64_t test_times = 5;

        for(unsigned i = 0; i < test_times; ++i)
        {
            checkVolume(v, 0, 4096, pattern);
        }

        VolManager::get()->getClusterCache().get_stats(hits, misses, entries);
#ifdef ENABLE_MD5_HASH
        EXPECT_EQ(1U, entries);
        EXPECT_EQ(test_times - 1, hits);
        EXPECT_EQ(1U, misses);
#else
        EXPECT_EQ(0U, entries);
        EXPECT_EQ(0U, hits);
        EXPECT_EQ(0U, misses);
#endif

        ip::PARAMETER_TYPE(clustercache_mount_points) mps(pt);

        for (const auto& mp: mps.value())
        {
            EXPECT_TRUE(device_info.count(mp.path) == 1);
            VolManager::get()->getClusterCache().offlineDevice(mp.path);
        }

        VolManager::get()->getClusterCache().deviceInfo(device_info);
        EXPECT_TRUE(device_info.size() == 0);

        VolManager::get()->getClusterCache().get_stats(hits, misses, entries);
#ifdef ENABLE_MD5_HASH
        EXPECT_EQ(0U, entries);
        EXPECT_EQ(test_times - 1, hits);
        EXPECT_EQ(1U, misses);
#else
        EXPECT_EQ(0U, entries);
        EXPECT_EQ(0U, hits);
        EXPECT_EQ(0U, misses);
#endif

        ConfigurationReport c_rep;
        ASSERT_TRUE(VolManager::get()->getClusterCache().checkConfig(pt,
                                                                c_rep));
        EXPECT_EQ(0U, c_rep.size());
        UpdateReport u_rep;

        for(unsigned i = 0; i < test_times; ++i)
        {
            checkVolume(v, 0, 4096, pattern);
        }

        VolManager::get()->getClusterCache().get_stats(hits, misses, entries);
#ifdef ENABLE_MD5_HASH
        EXPECT_EQ(0U, entries);
        EXPECT_EQ(test_times - 1, hits);
        EXPECT_EQ(test_times + 1, misses);
#else
        EXPECT_EQ(0U, entries);
        EXPECT_EQ(0U, hits);
        EXPECT_EQ(0U, misses);
#endif

        ASSERT_NO_THROW(VolManager::get()->getClusterCache().update(pt,
                                                               u_rep));
        VolManager::get()->getClusterCache().deviceInfo(device_info);
        EXPECT_TRUE(device_info.size() == 2);

        for (const auto& mp: mps.value())
        {
            EXPECT_TRUE(device_info.count(mp.path) == 1);
        }
        for(unsigned i = 0; i < test_times; ++i)
        {
            checkVolume(v, 0, 4096, pattern);
        }

        VolManager::get()->getClusterCache().get_stats(hits, misses, entries);
#ifdef ENABLE_MD5_HASH
        EXPECT_EQ(1U, entries);
        EXPECT_EQ(hits, 2*test_times - 2);
        EXPECT_EQ(misses, test_times + 2);
#else
        EXPECT_EQ(0U, entries);
        EXPECT_EQ(hits, 0);
        EXPECT_EQ(misses, 0U);
#endif
    }
}

TEST_P(VolumeDriverConfigurationTest, defaultNoDefault)
{
    bpt::ptree pt_no_default;
    bpt::ptree pt_with_default;
    VolManager::get()->persistConfiguration(pt_no_default, ReportDefault::F);
    VolManager::get()->persistConfiguration(pt_with_default, ReportDefault::T);
}

TEST_P(VolumeDriverConfigurationTest, num_threads)
{
    {
        ip::PARAMETER_TYPE(num_threads) val(350);
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);

        val.persist(pt);
        UpdateReport u_rep;
        ConfigurationReport c_rep;

        ASSERT_FALSE(VolManager::get()->updateConfiguration(pt,
                                                            u_rep,
                                                            c_rep));
        ASSERT_EQ(1U, c_rep.size());
        ASSERT_EQ(0U, u_rep.update_size());
        ASSERT_EQ(0U, u_rep.no_update_size());
    }

    {
        ip::PARAMETER_TYPE(num_threads) val(64);
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);

        val.persist(pt);
        UpdateReport u_rep;
        ConfigurationReport c_rep;
        ASSERT_TRUE(VolManager::get()->updateConfiguration(pt,
                                                           u_rep,
                                                           c_rep));
        EXPECT_EQ(0U, c_rep.size());
        EXPECT_EQ(1U, u_rep.update_size());

        for (const auto& u: u_rep.getUpdates())
        {
            LOG_INFO("updated: " << u.parameter_name);
        }

        EXPECT_EQ(40U,
                  u_rep.no_update_size());
    }

    {
        ip::PARAMETER_TYPE(num_threads) val(64);
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);
        val.persist(pt);
        UpdateReport u_rep;
        ConfigurationReport c_rep;
        ASSERT_TRUE(VolManager::get()->updateConfiguration(pt,
                                                           u_rep,
                                                           c_rep));
        EXPECT_EQ(0U, u_rep.update_size());

        for (const auto& u: u_rep.getUpdates())
        {
            std::cerr << u.parameter_name << std::endl;
        }

        EXPECT_EQ(41U,
                  u_rep.no_update_size());
    }
}

TEST_P(VolumeDriverConfigurationTest, enums)
{
    bpt::ptree pt;
    std::list<ClusterCacheBehaviour> lst{ ClusterCacheBehaviour::CacheOnWrite,
            ClusterCacheBehaviour::CacheOnRead,
            ClusterCacheBehaviour::NoCache
            };

    for (const auto& behaviour: lst)
    {
        ip::PARAMETER_TYPE(read_cache_default_behaviour)(behaviour).persist(pt);
        ip::PARAMETER_TYPE(read_cache_default_behaviour) read_cache(pt);
        ASSERT_EQ(read_cache.value(), behaviour);
    }
}

TEST_P(VolumeDriverConfigurationTest, enum_streams)
{
    const fs::path dump_file(FileUtils::create_temp_file_in_temp_dir("enum_dump1"));
    ALWAYS_CLEANUP_FILE(dump_file);

    bpt::ptree pt;
    ip::PARAMETER_TYPE(read_cache_default_behaviour)(ClusterCacheBehaviour::NoCache).persist(pt);
    bpt::json_parser::write_json(dump_file.string(),
                                 pt);

    bpt::ptree pt2;

    bpt::json_parser::read_json(dump_file.string(),
                                pt2);

    ip::PARAMETER_TYPE(read_cache_default_behaviour) read_cache(pt2);
    ASSERT_EQ(read_cache.value(), ClusterCacheBehaviour::NoCache);
}

TEST_P(VolumeDriverConfigurationTest, dump)
{
    const fs::path
        dump_file(FileUtils::create_temp_file_in_temp_dir("configuration_dump1"));

    ALWAYS_CLEANUP_FILE(dump_file);
    VolManager::get()->persistConfiguration(dump_file.string());

    UpdateReport report;
    ConfigurationReport c_rep;

    const boost::variant<yt::UpdateReport, yt::ConfigurationReport>
        res(VolManager::get()->updateConfiguration(dump_file));

    ASSERT_NO_THROW(boost::get<yt::UpdateReport>(res));
    ASSERT_THROW(boost::get<yt::ConfigurationReport>(res),
                 std::exception);

    const fs::path
        dump_file2(FileUtils::create_temp_file_in_temp_dir("configuration_dump2"));

    ALWAYS_CLEANUP_FILE(dump_file2);

    VolManager::get()->persistConfiguration(dump_file2);
}

TEST_P(VolumeDriverConfigurationTest, verify_property_tree)
{
    bpt::ptree pt;
    VolManager::get()->persistConfiguration(pt);

    yt::VolumeDriverComponent* comp = VolManager::get();
    const std::string cname(comp->componentName());

    EXPECT_NO_THROW(yt::VolumeDriverComponent::verify_property_tree(pt));

    {
        const std::string key("some-outdated-key-in-an-otherwise-valid-component");
        pt.put(cname + std::string(".") + key,
               "some-irrelevant-value");

        EXPECT_THROW(yt::VolumeDriverComponent::verify_property_tree(pt),
                     ip::ParameterInfo::UnknownParameterException);

        auto it = pt.find(cname);
        ASSERT_TRUE(it != pt.not_found());
        it->second.erase(key);

        EXPECT_NO_THROW(yt::VolumeDriverComponent::verify_property_tree(pt));
    }

    {
        const std::string key("some-outdated-component");
        pt.put(key + std::string(".some-outdated-key-in-that-outdated-component"),
               "some-uninteresting-value");
        EXPECT_THROW(yt::VolumeDriverComponent::verify_property_tree(pt),
                     ip::ParameterInfo::UnknownParameterException);
        pt.erase(key);
        EXPECT_NO_THROW(yt::VolumeDriverComponent::verify_property_tree(pt));
    }

    {
        // Some component completely absent in the config, used to trigger a segfault
        // in the binary, unfortunately not in the test
        bpt::ptree::size_type count =  pt.erase(cname);
        EXPECT_EQ(1U, count);
        auto it = pt.find(cname);
        EXPECT_TRUE(pt.not_found() == it);
        EXPECT_NO_THROW(yt::VolumeDriverComponent::verify_property_tree(pt));
    }
}

TEST_P(VolumeDriverConfigurationTest, obscenely_large_volume)
{
    const uint64_t hlimit =
        Entry::max_valid_cluster_address() *
        VolumeConfig::default_cluster_size() - VolManager::barts_correction;

    {
        ip::PARAMETER_TYPE(max_volume_size) val(hlimit + 1);
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);
        val.persist(pt);

        UpdateReport urep;
        ConfigurationReport crep;

        ASSERT_FALSE(VolManager::get()->updateConfiguration(pt, urep, crep));
        ASSERT_EQ(1U, crep.size());
        ASSERT_EQ(0U, urep.update_size());
    }

    {
        ip::PARAMETER_TYPE(max_volume_size) val(hlimit);
        bpt::ptree pt;
        VolManager::get()->persistConfiguration(pt);
        val.persist(pt);

        UpdateReport urep;
        ConfigurationReport crep;

        ASSERT_TRUE(VolManager::get()->updateConfiguration(pt, urep, crep));
        ASSERT_EQ(0U, crep.size());
        ASSERT_EQ(1U, urep.update_size());
    }
}

TEST_P(VolumeDriverConfigurationTest, open_scos_limits)
{
    const ip::PARAMETER_TYPE(open_scos_per_volume) too_small(0);
    test_forbidden_param_update(too_small);

    const ip::PARAMETER_TYPE(open_scos_per_volume) too_big(1024);
    test_forbidden_param_update(too_big);
}

TEST_P(VolumeDriverConfigurationTest, scos_per_tlog_limit)
{
    const ip::PARAMETER_TYPE(number_of_scos_in_tlog) n(0);
    test_forbidden_param_update(n);
}

TEST_P(VolumeDriverConfigurationTest, nondisposable_scos_factor_limit)
{
    const ip::PARAMETER_TYPE(non_disposable_scos_factor) f(0.9999);
    test_forbidden_param_update(f);
}

TEST_P(VolumeDriverConfigurationTest, no_reconfiguration_of_cluster_cache_mode)
{
    const ip::PARAMETER_TYPE(read_cache_default_mode) m(ClusterCacheMode::LocationBased);
    test_forbidden_param_update(m);
}

TEST_P(VolumeDriverConfigurationTest, no_reconfiguration_of_cluster_cache_behaviour)
{
    const ip::PARAMETER_TYPE(read_cache_default_behaviour)
        b(ClusterCacheBehaviour::CacheOnWrite);
    test_forbidden_param_update(b);
}

INSTANTIATE_TEST(VolumeDriverConfigurationTest);

}

// Local Variables: **
// mode: c++ **
// End: **
