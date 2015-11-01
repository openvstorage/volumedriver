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

#include "PrefetchData.h"
#include "VolManager.h"
#include "SCOCache.h"
#include "SCOFetcher.h"

namespace volumedriver
{

PrefetchData::PrefetchData()
    :VolumeBackPointer(getLogger__()),
     stop_(false)
{};

void
PrefetchData::initialize(Volume* v)
{
    setVolume(v);
}

void
PrefetchData::addSCO(SCO a,
                     float val)
{
    boost::lock_guard<boost::mutex> g(mut);
    if (not stop_)
    {
        scos.push(std::make_pair(a, val));
        cond.notify_one();
    }
}

void
PrefetchData::stop()
{
    boost::lock_guard<boost::mutex> g(mut);
    stop_ = true;
    while(not scos.empty())
    {
        scos.pop();
    }
    cond.notify_one();
}

void
PrefetchData::operator()()
{
    try
    {
        run_();
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Exception in prefetch thread: " << EWHAT <<
                      " - exiting");
            ASSERT(false);
        });
}

void
PrefetchData::run_()
{
    LOG_INFO("Starting prefetch thread");
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

            LOG_INFO("Prefetching sco " << getVolume()->getNamespace() << "/" <<
                     sconame << " with sap " << val);

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

            LOG_INFO("Prefetching sco " << getVolume()->getNamespace() << "/"
                     << sconame << " done, " <<
                     (res ? "continuing" : "stopping") << " prefetching");

            if(not res)
            {
                boost::lock_guard<boost::mutex> g(mut);
                while(not scos.empty())
                {
                    scos.pop();
                }
            }
        }
        else if(not stop_)
        {
            boost::unique_lock<boost::mutex> lock(mut);
            if(not stop_)
            {
                LOG_INFO("I'm waiting for the man");
                cond.wait(lock);
                LOG_INFO("Woke up too early, 't was a terrible mistake");
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
    LOG_INFO("Stopping prefetch thread ");
}

}

// Local Variables: **
// mode: c++ **
// End: **
