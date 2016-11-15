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

#include "Types.h"
#include "SCO.h"
#include "VolumeBackPointer.h"

#include <queue>
#include <utility>

#include <boost/thread.hpp>

#include <youtils/Logging.h>

namespace volumedriver
{

class VolManagerTestSetup;

class PrefetchData
    : public VolumeBackPointer
{
    friend class VolManagerTestSetup;

    using TP = std::pair<SCO, float>;

    struct Compare
    {
        bool
        operator()(const TP& first, const TP& second)
        {
            return first.second < second.second;
        }
    };

    using SCOQueue = std::priority_queue<TP, std::vector<TP>, Compare>;

public:
    explicit PrefetchData(Volume& v);

    ~PrefetchData();

    PrefetchData(const PrefetchData&) = delete;

    PrefetchData&
    operator=(const PrefetchData&) = delete;

    void
    addSCO(SCO a,
           float val);

private:
    DECLARE_LOGGER("PrefetchData");

    boost::mutex mut;
    SCOQueue scos;
    bool stop_;
    boost::condition_variable cond;
    boost::thread thread_;

    void
    run_();

    void
    clear_();
};

}

// Local Variables: **
// mode: c++ **
// End: **


#endif // PREFETCH_DATA_H
