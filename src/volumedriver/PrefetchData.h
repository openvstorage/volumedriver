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

#ifndef PREFETCH_DATA_H
#define PREFETCH_DATA_H

#include <vector>
#include <string>
#include <boost/thread.hpp>
#include "Types.h"
#include "NSIDMap.h"
#include <youtils/Logging.h>
#include "VolumeBackPointer.h"
#include <queue>
#include <utility>
#include "SCO.h"
namespace volumedriver
{

class VolManagerTestSetup;

class
Compare
{
public:
    typedef std::pair<SCO, float> TP;
    bool
    operator()(const TP& first, const TP& second)
    {
        return first.second < second.second;
    }

};

class PrefetchData : public VolumeBackPointer
{
    friend class VolManagerTestSetup;

public:
    typedef std::pair<SCO, float> TP;

    typedef std::priority_queue<TP, std::vector<TP>, Compare> SCOQueue;

    PrefetchData();

    void
    initialize(Volume* v);

    DECLARE_LOGGER("PrefetchData");

    void
    addSCO(SCO a,
           float val);

    void
    stop();

    void
    operator()();

private:
    void run_();
    bool stop_;
    SCOQueue scos;
    boost::mutex mut;
    boost::condition_variable cond;

};


}

// Local Variables: **
// mode: c++ **
// End: **


#endif // PREFETCH_DATA_H
