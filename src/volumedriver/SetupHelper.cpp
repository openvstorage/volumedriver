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

#include "SetupHelper.h"
#include <VolumeDriverParameters.h>
#include <backend/BackendParameters.h>
#include <boost/property_tree/json_parser.hpp>

namespace volumedriver
{
using namespace initialized_params;
namespace fs = boost::filesystem;
namespace bpt = boost::property_tree;
namespace yt = youtils;

namespace
{

template<typename T>
T
default_if_not_present(const bpt::ptree pt,
                       const typename T::ValueType& def)
{
    try
    {
        T t(pt);
        if (not t.was_defaulted())
        {
            return T(def);
        }
    }
    catch (MissingParameterException& e)
    {}

    return T(def);
}

}

SetupHelper::SetupHelper(const fs::path& json_config_file,
                         const fs::path& default_directory,
                         const UseClusterCache use_read_cache,
                         const CleanupLocalBackend cleanup_local_backend)
    : json_config_file_(json_config_file)
    , default_directory_(default_directory)
    , use_read_cache_(use_read_cache)
    , cleanup_local_backend_(cleanup_local_backend)
{}


void
SetupHelper::setup()
{
    bpt::ptree ptree;
    if(not json_config_file_.empty())
    {

        bpt::read_json(json_config_file_.string(),
                       ptree);

    }

    setup_scocache_mountpoints(ptree);
    setup_backend(ptree);
    setup_readcache_serialization(ptree);
    setup_volmanager_parameters(ptree);
    setup_threadpool(ptree);

}

void
SetupHelper::setup_scocache_mountpoints(const bpt::ptree& ptree)
{
    {
        using Param = PARAMETER_TYPE(scocache_mount_points);

        static MountPointConfigs default_scocache_mount_points =
            {
                { default_directory_ / "scocache_mount_points" / "mount_point_1",
                  youtils::DimensionedValue("1GiB").getBytes() },
                { default_directory_ / "scocache_mount_points" / "mount_point_2",
                  youtils::DimensionedValue("1GiB").getBytes() }
            };

        const Param smps(default_if_not_present<Param>(ptree,
                                                       default_scocache_mount_points));

        for(auto&& mp : smps.value())
        {
            FileUtils::ensure_directory(mp.path);
        }

        smps.persist(config_ptree_,
                     ReportDefault::T);
    }

    {
        PARAMETER_TYPE(datastore_throttle_usecs) the_datastore_throttle_usecs(ptree);
        the_datastore_throttle_usecs.persist(config_ptree_,
                                             ReportDefault::T);
    }

    {
        using Param = PARAMETER_TYPE(trigger_gap);
        static const yt::DimensionedValue default_trigger_gap("256MiB");

        const Param trigger_gap(default_if_not_present<Param>(ptree,
                                                              default_trigger_gap));
        trigger_gap.persist(config_ptree_,
                            ReportDefault::T);
    }

    {
        using Param = PARAMETER_TYPE(backoff_gap);
        static const yt::DimensionedValue default_backoff_gap("500MiB");

        const Param backoff_gap(default_if_not_present<Param>(ptree,
                                                              default_backoff_gap));
        backoff_gap.persist(config_ptree_,
                            ReportDefault::T);
    }
    {
        PARAMETER_TYPE(discount_factor) the_discount_factor(ptree);
        the_discount_factor.persist(config_ptree_,
                                    ReportDefault::T);
    }
}

void
SetupHelper::setup_backend(const bpt::ptree& ptree)
{
    auto child_ptree = ptree.get_child_optional(backend_connection_manager_name);

    if(not child_ptree )
    {
        PARAMETER_TYPE(backend_type)(backend::BackendType::LOCAL).persist(config_ptree_,
                                                                 ReportDefault::T);
        fs::path local_backend_path = default_directory_ / "localbackend";
        PARAMETER_TYPE(local_connection_path)(local_backend_path.string()).persist(config_ptree_,
                                                                                   ReportDefault::T);
    }
    else
    {
        ASSERT(child_ptree);
        config_ptree_.put_child(backend_connection_manager_name,
                               *child_ptree);
    }
    if(T(cleanup_local_backend_))
    {
        PARAMETER_TYPE(backend_type) the_backend_type(ptree);
        if(the_backend_type.value() == backend::BackendType::LOCAL)
        {
            PARAMETER_TYPE(local_connection_path) the_local_connection_path(config_ptree_);
            const fs::path p(the_local_connection_path.value());
            LOG_INFO("Removing and recreating directory for localbackend " << p);
            fs::remove_all(p);
            fs::create_directories(p);
        }
    }
}


void
SetupHelper::setup_failovercache(const bpt::ptree& /*ptree*/)
{
}

void
SetupHelper::setup_readcache_serialization(const bpt::ptree& ptree)
{
    if(T(use_read_cache_))
    {
        {
            PARAMETER_TYPE(serialize_read_cache) the_serialize_read_cache(ptree);
            the_serialize_read_cache.persist(config_ptree_,
                                             ReportDefault::T);
        }
        {
            using Param = PARAMETER_TYPE(read_cache_serialization_path);

            static const fs::path default_read_cache_serialization_path =
                default_directory_ / "readcache_serialization_path";
            // Ensure that it's a file an precreate it??
            const Param path(default_if_not_present<Param>(ptree,
                                                           default_read_cache_serialization_path.string()));
            path.persist(config_ptree_,
                         ReportDefault::T);
        }

        {
            PARAMETER_TYPE(average_entries_per_bin) the_average_entries_per_bin(ptree);
            the_average_entries_per_bin.persist(config_ptree_,
                                                ReportDefault::T);
        }

        {
            static const yt::DimensionedValue read_cache_size("250MiB");

            static MountPointConfigs default_cluster_cache_mount_points =
            {
                { default_directory_ / "readcache_mount_points" / "mount_point_1",
                  read_cache_size.getBytes() },
                { default_directory_ / "readcache_mount_points" / "mount_point_2",
                  read_cache_size.getBytes() }
            };

            using Param = PARAMETER_TYPE(clustercache_mount_points);
            const auto cmps(default_if_not_present<Param>(ptree,
                                                          default_cluster_cache_mount_points));

            for(auto&& mp : cmps.value())
            {
                FileUtils::ensure_file(mp.path,
                                       mp.size);
            }

            cmps.persist(config_ptree_,
                         ReportDefault::T);
        }

    }
}

void
SetupHelper::setup_volmanager_parameters(const boost::property_tree::ptree& ptree)
{
    {
        static const fs::path default_tlog_path = default_directory_ / "tlogs" ;
        using Param = PARAMETER_TYPE(tlog_path);

        const auto path(default_if_not_present<Param>(ptree,
                                                      default_tlog_path.string()));
        FileUtils::ensure_directory(path.value());
        path.persist(config_ptree_,
                     ReportDefault::T);
    }
    {
        static const fs::path default_metadata_path = default_directory_ / "metadata";
        using Param = PARAMETER_TYPE(metadata_path);

        const auto path(default_if_not_present<Param>(ptree,
                                                      default_metadata_path.string()));
        FileUtils::ensure_directory(path.value());
        path.persist(config_ptree_,
                     ReportDefault::T);
    }
    {
        PARAMETER_TYPE(open_scos_per_volume) the_open_scos_per_volume(ptree);
        the_open_scos_per_volume.persist(config_ptree_,
                                         ReportDefault::T);
    }
    {
        PARAMETER_TYPE(dtl_throttle_usecs) the_dtl_throttle_usecs(ptree);
        the_dtl_throttle_usecs.persist(config_ptree_,
                                       ReportDefault::T);
    }
    {
        PARAMETER_TYPE(dtl_queue_depth) the_dtl_queue_depth(ptree);
        the_dtl_queue_depth.persist(config_ptree_,
                                    ReportDefault::T);
    }
    {
        PARAMETER_TYPE(dtl_write_trigger) the_dtl_write_trigger(ptree);
        the_dtl_write_trigger.persist(config_ptree_,
                                      ReportDefault::T);
    }

    {
        PARAMETER_TYPE(freespace_check_interval) the_freespace_check_interval(ptree);
        the_freespace_check_interval.persist(config_ptree_,
                                             ReportDefault::T);
    }

    {
        static const uint32_t sc_clean_interval_ = 60;
        using Param = PARAMETER_TYPE(clean_interval);
        const auto the_sc_clean_interval(default_if_not_present<Param>(ptree,
                                                                       sc_clean_interval_));
        the_sc_clean_interval.persist(config_ptree_,
                                      ReportDefault::T);

    }
    {
        PARAMETER_TYPE(sap_persist_interval) the_sap_persist_interval(ptree);
        the_sap_persist_interval.persist(config_ptree_,
                                         ReportDefault::T);
    }
    {
        PARAMETER_TYPE(dtl_check_interval_in_seconds)
            the_failover_check_interval_in_seconds(ptree);
        the_failover_check_interval_in_seconds.persist(config_ptree_,
                                                       ReportDefault::T);

    }
    {
        PARAMETER_TYPE(read_cache_default_behaviour) the_readcache_default_behaviour(ptree);
        the_readcache_default_behaviour.persist(config_ptree_,
                                                ReportDefault::T);
    }

    // TODO these should have a dimensioned value constructor.
    {
        PARAMETER_TYPE(required_meta_freespace) the_required_meta_freespace(ptree);
        the_required_meta_freespace.persist(config_ptree_,
                                            ReportDefault::T);
    }

    {
        PARAMETER_TYPE(required_tlog_freespace) the_required_tlog_freespace(ptree);
        the_required_tlog_freespace.persist(config_ptree_,
                                            ReportDefault::T);
    }
    {
        PARAMETER_TYPE(max_volume_size) the_max_volume_size(ptree);
        the_max_volume_size.persist(config_ptree_,
                                    ReportDefault::T);
    }
}


void
SetupHelper::setup_threadpool(const bpt::ptree& ptree)
{

    PARAMETER_TYPE(num_threads) the_num_threads(ptree);
    the_num_threads.persist(config_ptree_);
}


void
SetupHelper::dump_json(const fs::path& output)
{
    LOG_INFO("Dumping the new config to: " << output);
    if(config_ptree_.empty())
    {
        LOG_INFO("Dump your empty ptrees yourself...");
        throw SetupHelperExceptionConfigPtreeEmpty("empty ptree");
    }
    if(fs::exists(output))
    {
        LOG_INFO("Quickly, file exists, press C-g to avoid overwriting");
        sleep(1);
        LOG_INFO("Ah shucks, too late");
    }

    bpt::write_json(output.string(),
                    config_ptree_);

}

}
