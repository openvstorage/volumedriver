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

#ifndef VFS_STATS_COLLECTOR_H_
#define VFS_STATS_COLLECTOR_H_

#include <youtils/Logging.h>
#include <youtils/PeriodicAction.h>

namespace volumedriverfs
{

template<typename T>
struct DefaultStatsSerializer
{
    static std::string
    serialize(const T& val)
    {
        std::stringstream ss;
        ss << val;
        return ss.str();
    }
};

class StatsCollector
{
public:
    using PushFun = std::function<void(const std::string&)>;
    using PullFun = std::function<void(StatsCollector&)>;

    StatsCollector(const std::atomic<uint64_t>& pull_interval_secs,
                   std::vector<PullFun> pull_funs,
                   PushFun push_fun)
        : pull_funs_(std::move(pull_funs))
        , push_fun_(std::move(push_fun))
        , action_("StatsCollector",
                  boost::bind(&StatsCollector::work_,
                              this),
                  pull_interval_secs)
    {
        LOG_INFO("up and running");
    }

    virtual ~StatsCollector() = default;

    StatsCollector(const StatsCollector&) = delete;

    StatsCollector&
    operator=(const StatsCollector&)=  delete;

    template<typename T,
             typename Serializer = DefaultStatsSerializer<T>>
    void
    push(const T& val)
    {
        push_fun_(Serializer::serialize(val));
    }

private:
    DECLARE_LOGGER("VFSStatsCollector");

    std::vector<PullFun> pull_funs_;

    PushFun push_fun_;

    youtils::PeriodicAction action_;

    void
    work_()
    {
        for (auto& pull : pull_funs_)
        {
            pull(*this);
        }
    }
};

}

#endif // !VFS_STATS_COLLECTOR_H_
