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

#include "PrefetchData.h"
#include "SCOCache.h"
#include "SCOFetcher.h"
#include "VolManager.h"
#include "Volume.h"

#include <youtils/Catchers.h>

namespace volumedriver
{

PrefetchData::PrefetchData(Volume& v)
    : VolumeBackPointer(getLogger__())
    , stop_(false)
{
    setVolume(&v);

    thread_ = boost::thread([&]
                            {
                                try
                                {
                                    run_();
                                }
                                CATCH_STD_ALL_EWHAT({
                                        LOG_ERROR(getVolume()->getName() <<
                                                  ": exception in prefetch thread: " <<
                                                  EWHAT << " - exiting");
                                    });
                            });
}

PrefetchData::~PrefetchData()
{
    try
    {
        {
            boost::lock_guard<boost::mutex> g(mut);
            stop_ = true;
            clear_();
            cond.notify_one();
        }

        thread_.join();
    }
    CATCH_STD_ALL_LOG_IGNORE(getVolume()->getName() << ": failed to stop prefetch thread");
}

void
PrefetchData::addSCO(SCO a,
                     float val)
{
    boost::lock_guard<boost::mutex> g(mut);

    scos.push(std::make_pair(a, val));
    cond.notify_one();
}

void
PrefetchData::clear_()
{
    ASSERT_LOCKABLE_LOCKED(mut);

    while (not scos.empty())
    {
        scos.pop();
    }
}

void
PrefetchData::run_()
{
    LOG_INFO(getVolume()->getName() << ": starting prefetch thread");

    while(true)
    {
        if(not scos.empty())
        {
            SCO sconame;
            float val;
            {
                boost::lock_guard<boost::mutex> g(mut);

                if(scos.empty())
                {
                    continue;
                }
                sconame = scos.top().first;
                val = scos.top().second;
                scos.pop();
            }

            //Volume* vol = getVolume();

            LOG_INFO(getVolume()->getName() << "Prefetching SCO " << sconame << " with SAP " << val);

            ClusterLocation loc(sconame, 0);
            BackendInterfacePtr bi = getVolume()->getBackendInterface(loc.cloneID())->clone();
            BackendSCOFetcher fetcher(sconame,
                                      getVolume(),
                                      bi->clone(),
                                      false); // Don't signal an error if we can't get a sco
            const VolumeConfig cfg(getVolume()->get_config());
            const uint64_t scoSize = cfg.getSCOSize();
            bool res =
                VolManager::get()->getSCOCache()->prefetchSCO(getVolume()->getNamespace(),
                                                              sconame,
                                                              scoSize,
                                                              val,
                                                              fetcher);

            LOG_INFO(getVolume()->getName() << ": prefetching SCO " << sconame << " done, " <<
                     (res ? "continuing" : "stopping") << " prefetching");

            if(not res)
            {
                boost::lock_guard<boost::mutex> g(mut);
                clear_();
            }
        }
        else if(not stop_)
        {
            boost::unique_lock<boost::mutex> lock(mut);
            if(not stop_)
            {
                LOG_INFO(getVolume()->getName() << ": no prefetch work, going to sleep");
                cond.wait(lock);
                LOG_INFO(getVolume()->getName() << ": waking up");
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    LOG_INFO(getVolume()->getName() << ": stopping prefetch thread");
}

}

// Local Variables: **
// mode: c++ **
// End: **
