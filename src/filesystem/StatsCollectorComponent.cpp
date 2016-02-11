// Copyright 2016 iNuron NV
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

#include "DataPoints.h"

#include "StatsCollector.h"
#include "StatsCollectorComponent.h"

#include <boost/property_tree/ptree.hpp>

#include <youtils/Catchers.h>
#include <youtils/RedisUrl.h>
#include <youtils/RedisQueue.h>

#include <volumedriver/Api.h>
#include <volumedriver/VolManager.h>

namespace volumedriverfs
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace vd = volumedriver;
namespace yt = youtils;

namespace
{

DECLARE_LOGGER("StatsCollectorUtils");

#define LOCKVD()                                                        \
    std::lock_guard<fungi::Mutex> vdg__(api::getManagementMutex())

StatsCollector::PushFun
make_push_fun(const std::string& dst)
{
    StatsCollector::PushFun fun;

    if (dst.empty())
    {
        fun = [](const std::string& msg)
            {
                LOG_INFO(msg);
            };
    }
    else if (yt::RedisUrl::is_one(dst))
    {
        const auto url = boost::lexical_cast<yt::RedisUrl>(dst);
        std::shared_ptr<yt::RedisQueue> queue;
        fun = [queue, url](const std::string& msg) mutable
            {
                try
                {
                    if (not queue)
                    {
                        queue = std::make_shared<yt::RedisQueue>(url, 10);
                    }

                    queue->push(msg);
                }
                CATCH_STD_ALL_EWHAT({
                        LOG_ERROR("Failed to push to " << url << ": " << EWHAT);
                        queue.reset();
                    });
            };
    }
    else
    {
        throw fungi::IOException("unsupported StatsCollector destination",
                                 dst.c_str());
    }

    return fun;
}

void
pull_cluster_cache_data_point(StatsCollector& pusher)
{
    pusher.push(ClusterCacheDataPoint());
}

void
pull_cluster_cache_device_data_point(StatsCollector& pusher)
{
    vd::ClusterCache::ManagerType::Info info;
    api::getClusterCacheDeviceInfo(info);

    for (const auto& p : info)
    {
        pusher.push(ClusterCacheDeviceDataPoint(p.second));
    }
}

void
pull_cluster_cache_namespace_data_point(StatsCollector& pusher)
{
    vd::ClusterCache& cc = vd::VolManager::get()->getClusterCache();
    for (const auto& h : cc.list_namespaces())
    {
        try
        {
            pusher.push(ClusterCacheNamespaceDataPoint(h));
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to get cluster cache namespace counters for " << h);
    }
}

void
pull_sco_cache_device_data_point(StatsCollector& pusher)
{
    vd::SCOCacheMountPointsInfo info;
    api::getSCOCacheMountPointsInfo(info);

    for (const auto& p : info)
    {
        pusher.push(SCOCacheDeviceDataPoint(p.second));
    }
}

template<typename T>
void
for_each_volume(StatsCollector& pusher)
{
    std::list<vd::VolumeId> l;

    {
        LOCKVD();
        api::getVolumeList(l);
    }

    for (const auto& id : l)
    {
        try
        {
            pusher.push(T(id));
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to collect " << T::name << " for " << id);
    }
}

std::vector<StatsCollector::PullFun>
make_pull_funs()
{
    return std::vector<StatsCollector::PullFun>({
                pull_cluster_cache_data_point,
                pull_cluster_cache_device_data_point,
                pull_cluster_cache_namespace_data_point,
                pull_sco_cache_device_data_point,
                for_each_volume<SCOCacheNamespaceDataPoint>,
                for_each_volume<VolumeMetaDataStoreDataPoint>,
                for_each_volume<VolumeClusterCacheDataPoint>,
                for_each_volume<VolumeSCOCacheDataPoint>,
                for_each_volume<VolumePerformanceCountersDataPoint>
            });
}

}

#define LOCK()                                          \
    std::lock_guard<decltype(lock_)> splg__(lock_)

StatsCollectorComponent::StatsCollectorComponent(const bpt::ptree& pt,
                                                 const RegisterComponent registerizle)
    : yt::VolumeDriverComponent(registerizle,
                                pt)
    , stats_collector_interval_secs(pt)
    , stats_collector_destination(pt)
    , stats_collector_(std::make_unique<StatsCollector>(stats_collector_interval_secs.value(),
                                                        make_pull_funs(),
                                                        make_push_fun(stats_collector_destination.value())))
{
    LOG_INFO("up and running");
}

void
StatsCollectorComponent::persist(bpt::ptree& pt,
                                 const ReportDefault report_default) const
{
    stats_collector_interval_secs.persist(pt,
                                          report_default);
    stats_collector_destination.persist(pt,
                                        report_default);
}

void
StatsCollectorComponent::update(const bpt::ptree& pt,
                                yt::UpdateReport& urep)
{
    ip::PARAMETER_TYPE(stats_collector_interval_secs) interval(pt);
    if (not interval.value())
    {
        LOCK();

        stats_collector_ = nullptr;
    }
    else
    {
        ip::PARAMETER_TYPE(stats_collector_destination) dst(pt);
        if (dst.value() != stats_collector_destination.value())
        {
            std::vector<StatsCollector::PullFun> pull_funs(make_pull_funs());
            StatsCollector::PushFun push_fun(make_push_fun(dst.value()));

            LOCK();

            stats_collector_ = nullptr;
            stats_collector_ = std::make_unique<StatsCollector>(interval.value(),
                                                                std::move(pull_funs),
                                                                std::move(push_fun));
        }
    }

    stats_collector_interval_secs.update(pt,
                                         urep);

    stats_collector_destination.update(pt,
                                       urep);
}

bool
StatsCollectorComponent::checkConfig(const bpt::ptree& pt,
                                     yt::ConfigurationReport& crep) const
{
    bool res = true;

    {
        ip::PARAMETER_TYPE(stats_collector_destination) val(pt);

        try
        {
            ip::PARAMETER_TYPE(stats_collector_destination) val(pt);
            make_push_fun(val.value());
        }
        CATCH_STD_ALL_EWHAT({
                crep.push_front(yt::ConfigurationProblem(val.name(),
                                                         val.section_name(),
                                                         EWHAT));
                res = false;
            });
    }

    return res;
}

}
