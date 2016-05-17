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
