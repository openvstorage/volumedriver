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
